/*
 * Copyright (c) 2006-2008 Apple Inc. All rights reserved.
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
#ifndef _LOAD_SMBFS_H_
#define _LOAD_SMBFS_H_

#define SMBFS_LOAD_KEXT_BOOTSTRAP_NAME "com.apple.smbfs_load_kext"

typedef char *string_t;


#define KEXT_LOAD_PATH "/sbin/kextload"
#define SMB_KEXT_PATH "/System/Library/Extensions/smbfs.kext"
#define LOAD_SMBKEXT_PATH "/System/Library/Extensions/smbfs.kext/Contents/Resources/load_smbfs"

/* We only support the default code page currently */
#define SMBFS_DEFAULT_CODE_PAGE "default"

int load_encodings(char * localcs, const char *locale);

#endif /* _LOAD_SMBFS_H_ */
