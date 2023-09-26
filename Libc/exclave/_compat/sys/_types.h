/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#ifndef __COMPAT_SYS__TYPES_H_
#define __COMPAT_SYS__TYPES_H_

#if defined(ENABLE_EXCLAVE_STORAGE)

#include <stdint.h>

typedef int64_t       blkcnt_t;      /* total blocks */
typedef int32_t       blksize_t;     /* preferred block size */
typedef int32_t       dev_t;         /* dev_t */
typedef unsigned int  fsblkcnt_t;    /* Used by statvfs and fstatvfs */
typedef unsigned int  fsfilcnt_t;    /* Used by statvfs and fstatvfs */
typedef uint32_t      uid_t;         /* [???] user IDs */
typedef uint32_t      gid_t;         /* [???] process and group IDs */
typedef uint64_t      ino64_t;       /* [???] Used for 64 bit inodes */
typedef ino64_t       ino_t;         /* [???] Used for inodes */
typedef uint16_t      mode_t;        /* [???] Some file attributes */
typedef uint16_t      nlink_t;       /* link count */
typedef int64_t       off_t;         /* [???] Used for file sizes */

#endif /* ENABLE_EXCLAVE_STORAGE */

#endif /* __COMPAT_SYS__TYPES_H_ */
