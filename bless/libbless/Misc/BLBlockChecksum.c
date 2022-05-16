/*
 * Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
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
 *  BLBlockChecksum.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Feb 28 2002.
 *  Copyright (c) 2002-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLBlockChecksum.c,v 1.8 2006/02/20 22:49:56 ssen Exp $
 *
 */

#include <sys/types.h>

#include "bless_private.h"

/*
 * Taken from MediaKit. Used to checksum secondary loader
 * presently
 */

uint32_t BLBlockChecksum(const void *buf,uint32_t length)
{
  uint32_t          sum = 0;
  uint32_t          *s = (uint32_t *) buf;
  uint32_t          *t = s + length/4;

  while (s < t) {
    //      rotate 1 bit left and add bytes
    sum = ((sum >> 31) | (sum << 1)) + *s++;
  }
  return sum;
}

