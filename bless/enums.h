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
 *  enums.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Nov 14 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: enums.h,v 1.19 2003/08/04 06:38:45 ssen Exp $
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
    kgetboot,
    khelp,
    kinfo,
    klabel,
    klabelfile,
    kmount,
    kopenfolder,
    kquiet,
    kplist,
    ksave9,
    ksaveX,
    ksetboot,
    ksetOF,
    kstartupfile,
    ksystem,
    ksystemfile,
    kuse9,
    kverbose,
    kwrapper,
    kxcoff,
    kversion,
    klast
};

enum {
    mGlobal =		1 << 0,
    mInfo =		1 << 1,
    mDevice =	1 << 2,
    mFolder = 	1 << 3,
    mHidden =   1 << 4,
    mModeMask = 0xF,
};


enum { // mutually exclusive
	aNone = 		1 << 0,
	aRequired = 	1 << 1,
	aOptional = 	1 << 2,
};


#define kDefaultHFSLabel ("Mac OS X")
#define kMaxArgLength 2048
