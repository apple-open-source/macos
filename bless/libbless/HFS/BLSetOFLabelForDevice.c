/*
 * Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
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
 *  BLSetOFLabelForDevice.c
 *  bless
 *
 *  Created by Shantonu Sen on Wed Apr 16 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLSetOFLabelForDevice.c,v 1.6 2003/08/04 05:24:16 ssen Exp $
 *
 *  $Log: BLSetOFLabelForDevice.c,v $
 *  Revision 1.6  2003/08/04 05:24:16  ssen
 *  Add #ifndef _OPEN_SOURCE so that some stuff isn't in darwin
 *
 *  Revision 1.5  2003/07/22 15:58:31  ssen
 *  APSL 2.0
 *
 *  Revision 1.4  2003/04/23 00:07:51  ssen
 *  Use blostype2string for OSTypes
 *
 *  Revision 1.3  2003/04/19 00:11:08  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.2  2003/04/17 00:18:26  ssen
 *  Make compile
 *
 *  Revision 1.1  2003/04/16 23:54:28  ssen
 *  Add new BLSetOFLabelForDevice to set OF Label for unmounted
 *  HFS+ partitions
 *
 *
 */

