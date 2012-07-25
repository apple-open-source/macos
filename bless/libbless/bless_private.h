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
 *  bless_private.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Feb 28 2002.
 *  Copyright (c) 2002-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: bless_private.h,v 1.22 2006/05/31 22:30:26 ssen Exp $
 *
 */

#ifndef _BLESS_PRIVATE_H_
#define _BLESS_PRIVATE_H_

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/cdefs.h>
#include <TargetConditionals.h>
#include <AvailabilityMacros.h>

#include "bless.h"

#define kBootBlocksSize 1024
#define kBootBlockTradOSSig 0x4c4b

#ifndef BLESS_EMBEDDED
#if defined(TARGET_OS_EMBEDDED) && TARGET_OS_EMBEDDED
#define BLESS_EMBEDDED 1
#else
#define BLESS_EMBEDDED 0
#endif
#endif

#if BLESS_EMBEDDED

#define USE_DISKARBITRATION		0
#define USE_CORETEXT			0
#define USE_COREGRAPHICS		0
#define USE_MEDIAKIT			0
#define SUPPORT_RAID			0
#define	SUPPORT_APPLE_PARTITION_MAP	0
#define SUPPORT_CSM_LEGACY_BOOT 0

#else

#define USE_DISKARBITRATION		1
#if defined(MAC_OS_X_VERSION_10_5) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5)
#define USE_CORETEXT			1
#else
#define USE_CORETEXT			0
#endif
#define USE_COREGRAPHICS		1
#define USE_MEDIAKIT			1
#define SUPPORT_RAID			1
#define	SUPPORT_APPLE_PARTITION_MAP	1
#define SUPPORT_CSM_LEGACY_BOOT 1

#endif

#if DISABLE_DISK_ARBITRATION
#undef  USE_DISKARBITRATION
#define USE_DISKARBITRATION		0
#endif

/*
 * Internal support for EFI routines shared between tool & library.
 */
int setefidevice(BLContextPtr context, const char * bsdname, int bootNext,
                 int bootLegacy, const char *legacyHint, 
				 const char *optionalData, bool shortForm);
int setefifilepath(BLContextPtr context, const char * path, int bootNext,
                                   const char *optionalData, bool shortForm);
int setefinetworkpath(BLContextPtr context, CFStringRef booterXML,
                      CFStringRef kernelXML, CFStringRef mkextXML,
					  CFStringRef kernelcacheXML, int bootNext);
int efinvramcleanup(BLContextPtr context);
int setit(BLContextPtr context, mach_port_t masterPort, const char *bootvar,
		  CFStringRef xmlstring);
int _forwardNVRAM(BLContextPtr context, CFStringRef from, CFStringRef to);


/* Calculate a shift-1-left & add checksum of all
 * 32-bit words
 */
uint32_t BLBlockChecksum(const void *buf , uint32_t length);

/*
 * write the CFData to a file
 */
int BLCopyFileFromCFData(BLContextPtr context, const CFDataRef data,
	     				 const char * dest, int shouldPreallocate);

/*
 * convert to a char * description
 */
char *BLGetCStringDescription(CFTypeRef typeRef);

/*
 * check if the context is null. if not, check if the log funcion is null
 */
int contextprintf(BLContextPtr context, int loglevel, char const *fmt, ...) __printflike(3, 4);;

/*
 * stringify the OSType into the caller-provided buffer
 */
char * blostype2string(uint32_t type, char buf[5]);

// statfs wrapper that works in single user mode,
// where the mount table hasn't been updated with the
// proper dev node
int blsustatfs(const char *path, struct statfs *buf);

#endif // _BLESS_PRIVATE_H_
