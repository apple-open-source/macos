/* 
 * Copyright (c) 1999-2004 Apple Computer, Inc.  All Rights Reserved.
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
 
#if !defined(__FILESYSTEM_PRIVATE__)
#define __FILESYSTEM_PRIVATE__ 1

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Input:
 * 	1. CFURLRef url: CFURLRef representation of current volume path
 * Output:
 * 	1. CFStringRef: Localized format name for given file system path
 *
 * This function returns the localized format name for given path in 
 * file system.
 */
extern CFStringRef _FSCopyLocalizedNameForVolumeFormatAtURL(CFURLRef url);

/* Input:
 * 	1. CFURLRef url: CFURLRef representation of current volume path
 * Output:
 * 	1. CFStringRef: English format name for given file system path
 *
 * This function returns the English format name for given path in 
 * file system.
 */
extern CFStringRef _FSCopyNameForVolumeFormatAtURL(CFURLRef url);

/* Input: 
 * 	1. CFStringRef devnode: CFStringRef representation of /dev/diskXsXX
 * Output:
 * 	1. CFStringRef: Localized format name for given /dev/diskXsXX
 * 		It returns "Unknown ( )" for the following conditions:
 * 			1. If the devnode is mounted already.
 *			2. If the file system is not HFS or MSDOS. 
 * 
 * This function returns the Localized format name for /dev/diskXsXX 
 */
extern CFStringRef _FSCopyLocalizedNameForVolumeFormatAtNode(CFStringRef devnode);

/* Input: 
 * 	1. CFStringRef devnode: CFStringRef representation of /dev/diskXsXX
 * Output:
 * 	1. CFStringRef: English format name for given /dev/diskXsXX
 * 		It returns "Unknown ( )" for the following conditions:
 * 			1. If the devnode is mounted already.
 *			2. If the file system is not HFS or MSDOS. 
 * 
 * This function returns the English format name for /dev/diskXsXX 
 */
extern CFStringRef _FSCopyNameForVolumeFormatAtNode(CFStringRef devnode);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__FILESYSTEM_PRIVATE__ */
