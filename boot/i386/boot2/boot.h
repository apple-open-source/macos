/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1994 NeXT Computer, Inc.
 * All rights reserved.
 */

#ifndef __BOOT2_BOOT_H
#define __BOOT2_BOOT_H

/*
 * Keys used in system Default.table / Instance0.table
 */
#define kGraphicsModeKey    "Graphics Mode"
#define kBootGraphicsKey    "Boot Graphics"
#define kQuietBootKey       "Quiet Boot"
#define kKernelFlagsKey     "Kernel Flags"
#define kKernelNameKey      "Kernel"

/*
 * Possible values for the bootdev argument passed to boot().
 */
enum {
	kBootDevHardDisk = 0,
	kBootDevFloppyDisk,
	kBootDevNetwork
};

/*
 * A global set by boot() to record the device that the booter
 * was loaded from.
 */
extern int gBootDev;

/*
 * graphics.c
 */
extern void message(char * str, int centered);
extern void setMode(int mode);
extern int  currentMode();
extern void spinActivityIndicator();
extern void clearActivityIndicator();

/*
 * drivers.c
 */
extern long LoadDrivers(char * dirSpec);

#endif /* !__BOOT2_BOOT_H */
