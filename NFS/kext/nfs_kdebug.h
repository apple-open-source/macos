/*
 * Copyright (c) 1999-2010 Apple Inc. All rights reserved.
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
/*
 * Copyright (c) 1989, 1993, 1995
 *    The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors.
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
 */

#ifndef _NFS_KDEBUG_H_
#define _NFS_KDEBUG_H_

#include <sys/kdebug.h>
#include "nfs_client.h"

#ifdef DBG_NFS
#define NFS_ENABLE_KDBG_TRACING    1
#endif

#if NFS_ENABLE_KDBG_TRACING

#define NFS_KDBG_ENTRY(code, ...)  do { \
	if (NFSCLNT_IS_DBG(NFSCLNT_FAC_KDBG, 10)) { \
	        KDBG_FILTERED((code) | DBG_FUNC_START, ##__VA_ARGS__); \
	} \
} while (0)

#define NFS_KDBG_EXIT(code, ...)  do { \
	if (NFSCLNT_IS_DBG(NFSCLNT_FAC_KDBG, 10)) { \
	        KDBG_FILTERED((code) | DBG_FUNC_END, ##__VA_ARGS__); \
	} \
} while (0)

#define NFS_KDBG_INFO(code, ...)  do { \
	if (NFSCLNT_IS_DBG(NFSCLNT_FAC_KDBG, 11)) { \
	        KDBG_FILTERED((code) | DBG_FUNC_NONE, ##__VA_ARGS__); \
	} \
} while (0)

#define NFS_KDBG_FILE_IO_RETURN do { \
    if (!NFSCLNT_IS_DBG(NFSCLNT_FAC_KDBG, 12)) { \
	return; \
    } \
} while (0);

/*
 * DBG_FSYSTEM (Class) = 0x3 and DBG_NFS (Subclass) = 0x15
 */
#define NFSDBG_CODE(code)  FSDBG_CODE(DBG_NFS, code)

/* nfs_vfsops.c tracepoints */
#define NFSDBG_VF_INIT                NFSDBG_CODE(0)     /* 0x3150000 */
#define NFSDBG_VF_MOUNT               NFSDBG_CODE(1)     /* 0x3150004 */
#define NFSDBG_VF_UNMOUNT             NFSDBG_CODE(2)     /* 0x3150008 */
#define NFSDBG_VF_ROOT                NFSDBG_CODE(3)     /* 0x315000c */
#define NFSDBG_VF_GETATTR             NFSDBG_CODE(4)     /* 0x3150010 */
#define NFSDBG_VF_SYNC                NFSDBG_CODE(5)     /* 0x3150014 */
#define NFSDBG_VF_SYSCTL              NFSDBG_CODE(6)     /* 0x3150018 */

/* nfs_vnops.c common tracepoints */
#define NFSDBG_VN_LOOKUP              NFSDBG_CODE(16)    /* 0x3150040 */
#define NFSDBG_VN_OPEN                NFSDBG_CODE(17)    /* 0x3150044 */
#define NFSDBG_VN_CLOSE               NFSDBG_CODE(18)    /* 0x3150048 */
#define NFSDBG_VN_ACCESS              NFSDBG_CODE(19)    /* 0x315004c */
#define NFSDBG_VN_SETATTR             NFSDBG_CODE(20)    /* 0x3150050 */
#define NFSDBG_VN_READ                NFSDBG_CODE(21)    /* 0x3150054 */
#define NFSDBG_VN_WRITE               NFSDBG_CODE(22)    /* 0x3150058 */
#define NFSDBG_VN_IOCTL               NFSDBG_CODE(23)    /* 0x315005c */
#define NFSDBG_VN_MMAP                NFSDBG_CODE(24)    /* 0x3150060 */
#define NFSDBG_VN_MMAP_CHECK          NFSDBG_CODE(25)    /* 0x3150064 */
#define NFSDBG_VN_MNOMAP              NFSDBG_CODE(26)    /* 0x3150068 */
#define NFSDBG_VN_FSYNC               NFSDBG_CODE(27)    /* 0x315006c */
#define NFSDBG_VN_REMOVE              NFSDBG_CODE(28)    /* 0x3150070 */
#define NFSDBG_VN_RENAME              NFSDBG_CODE(29)    /* 0x3150074 */
#define NFSDBG_VN_READDIR             NFSDBG_CODE(30)    /* 0x3150078 */
#define NFSDBG_VN_READLINK            NFSDBG_CODE(31)    /* 0x315007c */
#define NFSDBG_VN_INACTIVE            NFSDBG_CODE(32)    /* 0x3150080 */
#define NFSDBG_VN_RECLAIM             NFSDBG_CODE(33)    /* 0x3150084 */
#define NFSDBG_VN_PATHCONF            NFSDBG_CODE(34)    /* 0x3150088 */
#define NFSDBG_VN_ADVLOCK             NFSDBG_CODE(35)    /* 0x315008c */
#define NFSDBG_VN_PAGEIN              NFSDBG_CODE(36)    /* 0x3150090 */
#define NFSDBG_VN_PAGEOUT             NFSDBG_CODE(37)    /* 0x3150094 */
#define NFSDBG_VN_BLKTOOFF            NFSDBG_CODE(38)    /* 0x3150098 */
#define NFSDBG_VN_OFFTOBLK            NFSDBG_CODE(39)    /* 0x315009c */
#define NFSDBG_VN_MONITOR             NFSDBG_CODE(40)    /* 0x31500a0 */

/* NFSv3 tracepoints */
#define NFSDBG_VN3_CREATE             NFSDBG_CODE(64)    /* 0x3150100 */
#define NFSDBG_VN3_MKNOD              NFSDBG_CODE(65)    /* 0x3150104 */
#define NFSDBG_VN3_GETATTR            NFSDBG_CODE(66)    /* 0x3150108 */
#define NFSDBG_VN3_LINK               NFSDBG_CODE(67)    /* 0x315010c */
#define NFSDBG_VN3_MKDIR              NFSDBG_CODE(68)    /* 0x3150110 */
#define NFSDBG_VN3_RMDIR              NFSDBG_CODE(69)    /* 0x3150114 */
#define NFSDBG_VN3_SYMLINK            NFSDBG_CODE(70)    /* 0x3150118 */

/* NFSv4 tracepoints */
#define NFSDBG_VN4_CREATE             NFSDBG_CODE(128)   /* 0x3150200 */
#define NFSDBG_VN4_MKNOD              NFSDBG_CODE(129)   /* 0x3150204 */
#define NFSDBG_VN4_GETATTR            NFSDBG_CODE(130)   /* 0x3150208 */
#define NFSDBG_VN4_LINK               NFSDBG_CODE(131)   /* 0x315020c */
#define NFSDBG_VN4_MKDIR              NFSDBG_CODE(132)   /* 0x3150210 */
#define NFSDBG_VN4_RMDIR              NFSDBG_CODE(133)   /* 0x3150214 */
#define NFSDBG_VN4_SYMLINK            NFSDBG_CODE(134)   /* 0x3150218 */
#define NFSDBG_VN4_GETXATTR           NFSDBG_CODE(135)   /* 0x315021c */
#define NFSDBG_VN4_SETXATTR           NFSDBG_CODE(136)   /* 0x3150220 */
#define NFSDBG_VN4_REMOVEXATTR        NFSDBG_CODE(137)   /* 0x3150224 */
#define NFSDBG_VN4_LISTXATTR          NFSDBG_CODE(138)   /* 0x3150228 */

#if NAMEDSTREAMS
#define NFSDBG_VN4_GETNAMEDSTREAM     NFSDBG_CODE(139)   /* 0x315022c */
#define NFSDBG_VN4_MAKENAMEDSTREAM    NFSDBG_CODE(140)   /* 0x3150230 */
#define NFSDBG_VN4_REMOVENAMEDSTREAM  NFSDBG_CODE(141)   /* 0x3150234 */
#endif /* NAMEDSTREAMS */

/* NFSv4 Callback tracepoints */
#define NFSDBG_CB4_HANDLER            NFSDBG_CODE(150)   /* 0x3150258 */
#define NFSDBG_CB4_RCV                NFSDBG_CODE(151)   /* 0x315025c */
#define NFSDBG_CB4_ACCEPT             NFSDBG_CODE(152)   /* 0x3150260 */
#define NFSDBG_CB4_MOUNT_SETUP        NFSDBG_CODE(153)   /* 0x3150264 */

/* nfs_bio.c tracepoints */
#define NFSDBG_BIO_UPL_SETUP          NFSDBG_CODE(160)   /* 0x3150280 */
#define NFSDBG_BIO_UPL_CHECK          NFSDBG_CODE(161)   /* 0x3150284 */
#define NFSDBG_BIO_PAGE_INVAL         NFSDBG_CODE(162)   /* 0x3150288 */
#define NFSDBG_BIO_BUF_FREEUP         NFSDBG_CODE(163)   /* 0x315028c */
#define NFSDBG_BIO_BUF_INCORE         NFSDBG_CODE(164)   /* 0x3150290 */
#define NFSDBG_BIO_BUF_MAP            NFSDBG_CODE(165)   /* 0x3150294 */
#define NFSDBG_BIO_BUF_GET            NFSDBG_CODE(166)   /* 0x3150298 */
#define NFSDBG_BIO_BUF_RELEASE        NFSDBG_CODE(167)   /* 0x315029c */
#define NFSDBG_BIO_BUF_IOWAIT         NFSDBG_CODE(168)   /* 0x31502a0 */
#define NFSDBG_BIO_BUF_IODONE         NFSDBG_CODE(169)   /* 0x31502a4 */
#define NFSDBG_BIO_BUF_WRITE_DELAYED  NFSDBG_CODE(170)   /* 0x31502a8 */
#define NFSDBG_BIO_BIOREAD            NFSDBG_CODE(171)   /* 0x31502ac */
#define NFSDBG_BIO_BUF_WRITE          NFSDBG_CODE(172)   /* 0x31502b0 */
#define NFSDBG_BIO_BUF_FLUSH_COMMITS  NFSDBG_CODE(173)   /* 0x31502b4 */
#define NFSDBG_BIO_BUF_FLUSH          NFSDBG_CODE(174)   /* 0x31502b8 */
#define NFSDBG_BIO_BUF_VINVALBUF      NFSDBG_CODE(175)   /* 0x31502bc */
#define NFSDBG_BIO_BUF_VINVALBUF2     NFSDBG_CODE(176)   /* 0x31502c0 */
#define NFSDBG_BIO_BUF_ASYNCIO_FINISH NFSDBG_CODE(177)   /* 0x31502c4 */

/* Other tracepoints */
#define NFSDBG_OP_LOADATTRCACHE       NFSDBG_CODE(192)   /* 0x3150300 */
#define NFSDBG_OP_GETATTRCACHE        NFSDBG_CODE(193)   /* 0x3150304 */
#define NFSDBG_OP_REQ_MATCH_REPLY     NFSDBG_CODE(194)   /* 0x3150308 */
#define NFSDBG_OP_REQ_FINISH          NFSDBG_CODE(195)   /* 0x315030c */
#define NFSDBG_OP_REQ_2               NFSDBG_CODE(196)   /* 0x3150310 */
#define NFSDBG_OP_REQ_GSS             NFSDBG_CODE(197)   /* 0x3150314 */
#define NFSDBG_OP_REQ_ASYNC           NFSDBG_CODE(198)   /* 0x3150318 */
#define NFSDBG_OP_REQ_ASYNC_FINISH    NFSDBG_CODE(199)   /* 0x315031c */
#define NFSDBG_OP_REQ_ASYNC_CANCEL    NFSDBG_CODE(200)   /* 0x3150320 */
#define NFSDBG_OP_SOFT_TERM           NFSDBG_CODE(201)   /* 0x3150324 */
#define NFSDBG_OP_NGET                NFSDBG_CODE(202)   /* 0x3150328 */
#define NFSDBG_OP_NODE_LOCK           NFSDBG_CODE(203)   /* 0x315032c */
#define NFSDBG_OP_NODE_UNLOCK         NFSDBG_CODE(204)   /* 0x3150330 */
#define NFSDBG_OP_DATA_LOCK           NFSDBG_CODE(205)   /* 0x3150334 */
#define NFSDBG_OP_DATA_UNLOCK         NFSDBG_CODE(206)   /* 0x3150338 */
#define NFSDBG_OP_DATA_UPDATE_SIZE    NFSDBG_CODE(207)   /* 0x315033c */
#define NFSDBG_OP_RPC_COMMIT_V3       NFSDBG_CODE(208)   /* 0x3150340 */
#define NFSDBG_OP_RPC_COMMIT_V4       NFSDBG_CODE(209)   /* 0x3150344 */
#define NFSDBG_OP_RPC_READ            NFSDBG_CODE(210)   /* 0x3150348 */
#define NFSDBG_OP_RPC_WRITE           NFSDBG_CODE(211)   /* 0x315034c */
#define NFSDBG_OP_GETATTR_INTERNAL    NFSDBG_CODE(212)   /* 0x3150350 */
#define NFSDBG_OP_SILLY_RENAME        NFSDBG_CODE(213)   /* 0x3150354 */
#define NFSDBG_OP_DATA_LOCK_STOE      NFSDBG_CODE(214)   /* 0x3150358 */
#define NFSDBG_OP_DATA_LOCK_ETOS      NFSDBG_CODE(215)   /* 0x315035c */
#define NFSDBG_OP_SEND_SNDLOCK_TRY    NFSDBG_CODE(216)   /* 0x3150360 */
#define NFSDBG_OP_SEND_SNDLOCK_LOCK   NFSDBG_CODE(217)   /* 0x3150364 */
#define NFSDBG_OP_SEND_SENDMBUF       NFSDBG_CODE(218)   /* 0x3150368 */

#else   /* !NFS_ENABLE_KDBG_TRACING */

#define NFS_KDBG_ENTRY(code, ...)  do {} while (0)
#define NFS_KDBG_EXIT(code, ...)   do {} while (0)
#define NFS_KDBG_INFO(code, ...)   do {} while (0)

#endif /* NFS_ENABLE_KDBG_TRACING */

void nfs_kdebug_io_start(mount_t, void *, size_t, uint32_t, int64_t);
void nfs_kdebug_io_end(vnode_t, void *, int64_t, int);

#endif  /* _NFS_KDEBUG_H_ */
