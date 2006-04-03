/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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
 *  bless_private.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Feb 28 2002.
 *  Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: bless_private.h,v 1.21 2005/11/15 23:59:52 ssen Exp $
 *
 */

#ifndef _BLESS_PRIVATE_H_
#define _BLESS_PRIVATE_H_

#include <sys/types.h>
#include <sys/mount.h>

#include "bless.h"

#define kBootBlocksSize 1024
#define kBootBlockTradOSSig 0x4c4b

/* Calculate a shift-1-left & add checksum of all
 * 32-bit words
 */
uint32_t BLBlockChecksum(const void *buf , uint32_t length);

/*
 * write the CFData to a file
 */
int BLCopyFileFromCFData(BLContextPtr context, const CFDataRef data,
	     const char * dest, int shouldPreallocate);


/*
 * check if the context is null. if not, check if the log funcion is null
 */
int contextprintf(BLContextPtr context, int loglevel, char const *fmt, ...)
    __attribute__ ((format (printf, 3, 4)));

/*
 * stringify the OSType into the caller-provided buffer
 */
char * blostype2string(uint32_t type, char buf[5]);

// statfs wrapper that works in single user mode,
// where the mount table hasn't been updated with the
// proper dev node
int blsustatfs(const char *path, struct statfs *buf);

#endif // _BLESS_PRIVATE_H_
