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
 *  dumpFI.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Apr 19 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: dumpFI.h,v 1.2 2002/02/23 04:13:05 ssen Exp $
 *
 *  $Log: dumpFI.h,v $
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

/* DONT USE THIS STRUCT */

/*
typedef struct {
  short   id;
  long   entryPoint;
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
*/
