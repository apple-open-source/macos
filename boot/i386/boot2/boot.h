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
 * How long to wait (in seconds) to load the
 * kernel after displaying the "boot:" prompt.
 */
#define BOOT_TIMEOUT     10      /* 10 second timeout */

/*
 * Keys used in system Default.table / Instance0.table
 */
#define PROMPT_KEY       "Prompt For Driver Disk"
#define NUM_PROMPTS_KEY  "Driver Disk Prompts"
#define ASK_KEY          "Ask For Drivers"
#define INSTALL_KEY      "Install Mode"
#define G_MODE_KEY       "Graphics Mode"

/*
 * Possible values for the bootdev argument passed to boot().
 */
enum {
	kBootDevHardDisk   = 0,
	kBootDevFloppyDisk = 1,
	kBootDevNetwork    = 2,
};

/*
 * The directory that contains the booter support files.
 */
#define BOOT_DIR_DISK       "/usr/standalone/i386/"
#define BOOT_DIR_NET        ""

/*
 * A global set by boot() to record the device that the booter
 * was loaded from.
 */
extern int gBootDev;

/*
 * Create the complete path to a booter support file.
 */
#define makeFilePath(x) \
    (gBootDev == kBootDevNetwork) ? BOOT_DIR_NET x : BOOT_DIR_DISK x

/*
 * Functions defined in graphics.c.
 */
extern void message(char * str, int centered);
extern void setMode(int mode);
extern int  currentMode();
extern void spinActivityIndicator();
extern void clearActivityIndicator();

#endif /* !__BOOT2_BOOT_H */
