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
/*
 *  BLSetActiveBIOSBootDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Jul 22 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLSetActiveBIOSBootDevice.c,v 1.6 2003/08/04 05:24:16 ssen Exp $
 *
 *  $Log: BLSetActiveBIOSBootDevice.c,v $
 *  Revision 1.6  2003/08/04 05:24:16  ssen
 *  Add #ifndef _OPEN_SOURCE so that some stuff isn't in darwin
 *
 *  Revision 1.5  2003/07/25 00:19:05  ssen
 *  add more apsl 2.0 license
 *
 *  Revision 1.4  2003/07/24 01:01:46  ssen
 *  grab MK defaults for read/writing pmap
 *
 *  Revision 1.3  2003/07/24 01:00:13  ssen
 *  Dumbass. open it read/write
 *
 *  Revision 1.2  2003/07/23 09:18:50  ssen
 *  Code to set active MBR partition
 *
 *
 */


#include "bless.h"
#include "bless_private.h"

int BLSetActiveBIOSBootDevice(BLContextPtr context, unsigned char device[]) {


    return 0;    
}

