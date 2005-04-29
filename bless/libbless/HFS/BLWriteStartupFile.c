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
 *  BLWriteStartupFile.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Mon Jun 25 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLWriteStartupFile.c,v 1.27 2005/02/03 00:42:26 ssen Exp $
 *
 *  $Log: BLWriteStartupFile.c,v $
 *  Revision 1.27  2005/02/03 00:42:26  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.26  2005/01/08 00:46:52  ssen
 *  <rdar://problem/3471757> bless -setBoot doesn't write booter
 *  When opening the whole device for fdisk maps (either -startupfile
 *  or -setBoot), tell MediaKit to open it with shared write access,
 *  so we can do it even with mounted partitions.
 *
 *  Revision 1.25  2004/04/20 21:40:42  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.24  2004/02/18 19:24:35  ssen
 *  <rdar://problem/3562918>: bless: Adopt MediaKit legacy headers
 *  Use #include <MediaKit/legacy/foo.h>
 *
 *  Revision 1.23  2003/10/16 23:50:05  ssen
 *  Partially finish cleanup of headers to add "const" to char[] arguments
 *  that won't be modified.
 *
 *  Revision 1.22  2003/07/22 15:58:31  ssen
 *  APSL 2.0
 *
 *  Revision 1.21  2003/07/02 06:26:31  ssen
 *  Add -eltorito. Purposefully underdocumented. Will re-evaluate later for a better home
 *
 *  Revision 1.20  2003/07/01 20:44:23  ssen
 *  new and improved BLWriteStartupFile that takes a CFDataRef and does the right thing
 *
 *  Revision 1.19  2003/07/01 01:35:31  ssen
 *  deprecated -xcoff in favor of -startupfile
 *
 *  Revision 1.18  2003/04/19 00:11:08  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.17  2003/04/16 23:57:31  ssen
 *  Update Copyrights
 *
 *  Revision 1.16  2003/04/12 03:52:19  ssen
 *  Straggling function prototype that still had void **data instead
 *  of CFDataRef *data
 *
 *  Revision 1.15  2003/03/26 00:33:29  ssen
 *  Use _OPEN_SOURCE_ instead of DARWIN, by Rob's request
 *
 *  Revision 1.14  2003/03/20 05:06:20  ssen
 *  remove some more non-c99 types
 *
 *  Revision 1.13  2003/03/19 22:57:02  ssen
 *  C99 types
 *
 *  Revision 1.12  2003/03/18 23:51:52  ssen
 *  Use MK directly
 *
 *  Revision 1.11  2003/03/08 18:25:07  ssen
 *  use the 64-bit ioctl to get device size
 *
 *  Revision 1.10  2002/12/05 03:37:54  ssen
 *  Wasnt updating the alt VH after adding a startup file. We were passing
 *  0 as the block count to MediaKit because we were using the 64-bit
 *  ioctl instead of 32-bit.
 *
 *  Revision 1.9  2002/12/04 05:02:25  ssen
 *  Move to using unifdef to strip out non-Darwin code
 *
 *  Revision 1.8  2002/06/11 00:50:46  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.7  2002/04/27 17:54:59  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.6  2002/04/25 07:27:27  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.5  2002/03/08 07:15:42  ssen
 *  Add #if !(_OPEN_SOURCE_), and add headerdoc
 *
 *  Revision 1.4  2002/03/05 00:01:42  ssen
 *  code reorg of secondary loader
 *
 *  Revision 1.3  2002/02/23 04:13:05  ssen
 *  Update to context-based API
 *
 *  Revision 1.2  2001/12/06 23:37:25  ssen
 *  For unpartitioned devices, don't try to update the pmap
 *
 *  Revision 1.1  2001/11/16 05:36:47  ssen
 *  Add libbless files
 *
 *  Revision 1.10  2001/11/11 06:19:08  ssen
 *  revert to -pre-libbless
 *
 *  Revision 1.8  2001/10/26 04:15:01  ssen
 *  add dollar Id and dollar Log
 *
 *
 */

