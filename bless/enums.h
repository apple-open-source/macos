/*
 * Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
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
 *  enums.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Nov 14 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: enums.h,v 1.37 2006/07/17 22:19:05 ssen Exp $
 *
 *
 */

#ifndef _ENUMS_H_
#define _ENUMS_H_

enum {
    kdummy = 0,
    kalternateos,
    kbootinfo,
	kbootefi,
    kbootblockfile,
    kbooter,
    kdevice,
    kfile,
	kfirmware,
    kfolder,
    kfolder9,
    kgetboot,
    khelp,
    kinfo,
    kkernel,
    kkernelcache,
    klabel,
    klabelfile,
    klegacy,
    klegacydrivehint,
    kmkext,
    kmount,
    knetboot,
    knextonly,
    kopenfolder,
    koptions,
    kpayload,
    kplist,
    kquiet,
	krecovery,
    kreset,
    ksave9,
    ksaveX,
    kserver,
    ksetboot,
    kshortform,
    kstartupfile,
    kunbless,
    kuse9,
    kverbose,
    kversion,
    klast
};

// getopt_long(3) uses ':' as a special return value
extern int too_many_options[klast >= ':' ? -1 : 0];

#define kMaxArgLength 2048

#endif // _ENUMS_H_
