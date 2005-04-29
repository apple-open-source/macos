/*
 * Copyright (c) 2003-2005 Apple Computer, Inc. All rights reserved.
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
 *  BLGetActiveBIOSPartitionForDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Mon Sep 22 2003.
 *  Copyright (c) 2003-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetActiveBIOSPartitionForDevice.c,v 1.5 2005/02/03 00:42:24 ssen Exp $
 *
 *  $Log: BLGetActiveBIOSPartitionForDevice.c,v $
 *  Revision 1.5  2005/02/03 00:42:24  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.4  2004/09/20 20:59:57  ssen
 *  <rdar://problem/3808101> bless (open source) shouldn't use CoreServices.h
 *
 *  Revision 1.3  2004/02/18 19:24:35  ssen
 *  <rdar://problem/3562918>: bless: Adopt MediaKit legacy headers
 *  Use #include <MediaKit/legacy/foo.h>
 *
 *  Revision 1.2  2003/10/17 00:10:39  ssen
 *  add more const
 *
 *  Revision 1.1  2003/09/23 04:56:03  ssen
 *  Add API to get active partition given a whole device, using MediaKit
 *
 */


#include "bless.h"
#include "bless_private.h"

int BLGetActiveBIOSPartitionForDevice(BLContextPtr context, const unsigned char device[],
				      unsigned char active[]) {

    int ret = 0;
    unsigned long slice = 0;

    
    sprintf(active, "%ss%lu", device, slice);
    
    return ret;    
}

