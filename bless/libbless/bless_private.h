/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 *  bless_private.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Feb 28 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: bless_private.h,v 1.2 2002/05/03 04:23:54 ssen Exp $
 *
 */

#include <sys/types.h>

#include "bless.h"

/* Calculate a shift-1-left & add checksum of all
 * 32-bit words
 */
u_int32_t BLBlockChecksum(const void *buf , u_int32_t length);

/*
 * write the CFData to a file
 */
int BLCopyFileFromCFData(BLContext context, void *data,
	     unsigned char dest[]);
