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
 *  dumpFI.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Apr 19 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: dumpFI.h,v 1.6 2003/07/22 15:58:30 ssen Exp $
 *
 *  $Log: dumpFI.h,v $
 *  Revision 1.6  2003/07/22 15:58:30  ssen
 *  APSL 2.0
 *
 *  Revision 1.5  2003/04/19 00:11:05  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.4  2003/04/16 23:57:30  ssen
 *  Update Copyrights
 *
 *  Revision 1.3  2003/03/24 19:03:48  ssen
 *  Use the BootBlock structure to get pertinent info form boot blocks
 *
 *  Revision 1.2  2002/02/23 04:13:05  ssen
 *  Update to context-based API
 *
 *  Revision 1.1  2001/11/16 05:36:46  ssen
 *  Add libbless files
 *
 *  Revision 1.4  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */

#include <CoreServices/CoreServices.h>

#pragma options align=mac68k

typedef struct {
  short   id;
  long    entryPoint;
  short   version;
  short   pageFlags;
  Str15   system;
  Str15   shellApplication;
  Str15   debugger1;
  Str15   debugger2;
  Str15   startupScreen;
  Str15   startupApplication;
  char    otherStuff[1024 - (2+4+2+2+16+16+16+16+16+16)];
} BootBlocks;

#pragma options align=reset