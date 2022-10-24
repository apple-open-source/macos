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

#include <sys/mount.h>
#include <stdbool.h>
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

#define FS_MEDIA_DEV_ENCRYPTED              0x1     // device-level encryption
#define FS_MEDIA_FDE_ENCRYPTED              0x2     // full-disk encryption
#define FS_MEDIA_ENCRYPTION_CONVERTING      0x4     // encryption type is currently in flux
typedef uint32_t fs_media_encryption_details_t;

/* Input:
 *  1. CFStringRef devnode: CFStringRef representation of /dev/diskXsXX
 *  2. bool *encryption_status: pointer to store boolean value of encryption status.
 *      Only valid on success. Must not be NULL.
 *  3. uint32_t *encryption_details: pointer to bitfield with extra encryption information.
 *      Only valid on success. May be NULL.
 * Output:
 *  1. errno_t: 0 upon success, or an errno indicating why no information could be found.
 *
 * This function returns the encryption status for /dev/diskXsXX
 */
extern errno_t _FSGetMediaEncryptionStatus(CFStringRef devnode, bool *encryption_status, fs_media_encryption_details_t *encryption_details);

/* Input:
 *  1. const char *devnode: C string representation of /dev/diskXsXX
 *  2. bool *encryption_status: pointer to store boolean value of encryption status.
 *      Only valid on success. Must not be NULL.
 *  3. uint32_t *encryption_details: pointer to bitfield with extra encryption information.
 *      Only valid on success. May be NULL.
 * Output:
 *  1. errno_t: 0 upon success, or an errno indicating why no information could be found.
 *
 * This function returns the encryption status for /dev/diskXsXX
 */
extern errno_t _FSGetMediaEncryptionStatusAtPath(const char *devnode, bool *encryption_status, fs_media_encryption_details_t *encryption_details);

/* Input:
 *  1. const char *path: C string representation of a file system path
 *  2. char *typenamebuf: Buffer where to store the file system type name.
 *     It is recommended that this buffer is at least MFSTYPENAMELEN bytes in size.
 *     May be NULL.
 *  3. size_t typenamebufsize: Size of typenamebuf.  Ignored if typenamebuf is NULL.
 *  4. uint32_t *subtypep: Pointer to a uint32_t that will contain the file
 *     system subtype, or 0 if no subtypes are defined for the file system.  May be NULL.
 *
 * Output:
 *  1. errno_t: 0 upon success, or an errno indicating the mode of failure.
 */
extern errno_t _FSGetTypeInfoForPath(const char *path, char *typenamebuf,
    size_t typenamebufsize, uint32_t *subtypep);

/* Input:
 *  1. int fd: A file descriptor representing an open file object.
 *  2. char *typenamebuf: Buffer where to store the file system type name.
 *     It is recommended that this buffer is at least MFSTYPENAMELEN bytes in size.
 *     May be NULL.
 *  3. size_t typenamebufsize: Size of typenamebuf.  Ignored if typenamebuf is NULL.
 *  4. uint32_t *subtypep: Pointer to a uint32_t that will contain the file
 *     system subtype, or 0 if no subtypes are defined for the file system.  May be NULL.
 *
 * Output:
 *  1. errno_t: 0 upon success, or an errno indicating the mode of failure.
 */
extern errno_t _FSGetTypeInfoForFileDescriptor(int fd, char *typenamebuf,
    size_t typenamebufsize, uint32_t *subtypep);

/* Input:
 *  1. const struct statfs *sfs: A buffer to a statfs structure filled out by statfs(2)
 *     or fstatfs(2).
 *  2. char *typenamebuf: Buffer where to store the file system type name.
 *     It is recommended that this buffer is at least MFSTYPENAMELEN bytes in size.
 *     May be NULL.
 *  3. size_t typenamebufsize: Size of typenamebuf.  Ignored if typenamebuf is NULL.
 *  4. uint32_t *subtypep: Pointer to a uint32_t that will contain the file
 *     system subtype, or 0 if no subtypes are defined for the file system.  May be NULL.
 *
 * Output:
 *  1. errno_t: 0 upon success, or an errno indicating the mode of failure.
 */
extern errno_t _FSGetTypeInfoFromStatfs(const struct statfs *sfs, char *typenamebuf,
    size_t typenamebufsize, uint32_t *subtypep);

/* Input:
 *  1. const char *path: C string representation of a file system path.
 *  2. char *locationbuf: Buffer where to store the file system location.
 *     It is recommended that this buffer is at least MNAMELEN bytes in size.
 *     May be NULL.
 *  3. size_t locationbufsize: Size of locationbuf.  Ignored if locationbuf is NULL.
 *
 * Output:
 *  1. errno_t: 0 upon success, or an errno indicating the mode of failure.
 */
extern errno_t _FSGetLocationForPath(const char *path, char *locationbuf,
    size_t locationbufsize);

/* Input:
 *  1. int fd: A file descriptor representing an open file object.
 *  2. char *locationbuf: Buffer where to store the file system location.
 *     It is recommended that this buffer is at least MNAMELEN bytes in size.
 *     May be NULL.
 *  3. size_t locationbufsize: Size of locationbuf.  Ignored if locationbuf is NULL.
 *
 * Output:
 *  1. errno_t: 0 upon success, or an errno indicating the mode of failure.
 */
extern errno_t _FSGetLocationForFileDescriptor(int fd, char *locationbuf,
    size_t locationbufsize);

/* Input:
 *  1. const struct statfs *sfs: A buffer to a statfs structure filled out by statfs(2)
 *     or fstatfs(2).
 *  2. char *locationbuf: Buffer where to store the file system location.
 *     It is recommended that this buffer is at least MNAMELEN bytes in size.
 *     May be NULL.
 *  3. size_t locationbufsize: Size of locationbuf.  Ignored if locationbuf is NULL.
 *
 * Output:
 *  1. errno_t: 0 upon success, or an errno indicating the mode of failure.
 */
extern errno_t _FSGetLocationFromStatfs(const struct statfs *sfs, char *locationbuf,
    size_t locationbufsize);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__FILESYSTEM_PRIVATE__ */
