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
 *  BLBlockChecksum.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Feb 28 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLBlockChecksum.c,v 1.1 2002/03/05 00:01:42 ssen Exp $
 *
 *  $Log: BLBlockChecksum.c,v $
 *  Revision 1.1  2002/03/05 00:01:42  ssen
 *  code reorg of secondary loader
 *
 *
 */

#include <sys/types.h>

#include "bless_private.h"

/*
 * Taken from MediaKit. Used to checksum secondary loader
 * presently
 */

u_int32_t BLBlockChecksum(const void *buf,u_int32_t length)
{
  u_int32_t          sum = 0;
  u_int32_t          *s = (u_int32_t *) buf;
  u_int32_t          *t = s + length/4;

  while (s < t) {
    //      rotate 1 bit left and add bytes
    sum = ((sum >> 31) | (sum << 1)) + *s++;
  }
  return sum;
}

