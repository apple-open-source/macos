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
 *  BLGenerateOFLabel.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Sat Feb 23 2002.
 *  Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGenerateOFLabel.c,v 1.16 2003/08/04 05:24:16 ssen Exp $
 *
 *  $Log: BLGenerateOFLabel.c,v $
 *  Revision 1.16  2003/08/04 05:24:16  ssen
 *  Add #ifndef _OPEN_SOURCE so that some stuff isn't in darwin
 *
 *  Revision 1.15  2003/07/22 15:58:34  ssen
 *  APSL 2.0
 *
 *  Revision 1.14  2003/04/19 00:11:12  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.13  2003/04/17 00:59:36  ssen
 *  truncate to 341 pixels if too wide
 *
 *  Revision 1.12  2003/04/16 23:57:33  ssen
 *  Update Copyrights
 *
 *  Revision 1.11  2003/03/26 00:33:31  ssen
 *  Use _OPEN_SOURCE_ instead of DARWIN, by Rob's request
 *
 *  Revision 1.10  2003/03/20 03:41:03  ssen
 *  Merge in from PR-3202649
 *
 *  Revision 1.9.2.1  2003/03/20 03:32:36  ssen
 *  swap height and width of OF label header
 *
 *  Revision 1.9  2003/03/19 22:57:06  ssen
 *  C99 types
 *
 *  Revision 1.7  2003/03/18 23:51:31  ssen
 *  Use CG directory
 *
 *  Revision 1.6  2002/06/11 00:50:49  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.5  2002/05/03 04:23:55  ssen
 *  Consolidate APIs, and update bless to use it
 *
 *  Revision 1.4  2002/04/27 17:55:00  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.3  2002/03/05 01:47:53  ssen
 *  Add CG compat files and dynamic loading
 *
 *  Revision 1.2  2002/03/04 22:25:05  ssen
 *  implement CLUT for antialiasing
 *
 *  Revision 1.1  2002/02/24 11:30:52  ssen
 *  Add OF label support
 *
 */

