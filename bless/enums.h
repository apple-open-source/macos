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
 *  enums.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Nov 14 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: enums.h,v 1.5 2002/05/29 01:13:58 ssen Exp $
 *
 *
 */

#include <sys/attr.h>

enum {
    kbootinfo = 0,
    kbootblocks,
    kbootblockfile,
    kdevice,
    kfolder,
    kfolder9,
    kformat,
    kfsargs,
    kinfo,
    klabel,
    kmount,
    kquiet,
    kplist,
    ksave9,
    ksaveX,
    ksetOF,
    ksystem,
    ksystemfile,
    kuse9,
    kverbose,
    kwrapper,
    kxcoff,
    klast
};

enum {
	mInfo =		1 << 0,
	mDevice =	1 << 1,
	mFolder = 	1 << 2,
	mModeMask = 0x7,
	mModeFlag = 1 << 7, // the flag that switches mode has this option
};


enum { // mutually exclusive
	aNone = 		1 << 0,
	aRequired = 	1 << 1,
	aOptional = 	1 << 2,
};


#define SYSTEM "\pSystem"
#define OSXBOOTFILE "OSXBoot!"
#define kDefaultHFSLabel ("Mac OS X")
#define kMaxArgLength 2048
