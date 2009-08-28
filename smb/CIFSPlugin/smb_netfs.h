/*
 * Copyright (c) 2006 - 2007 Apple Inc. All rights reserved.
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

#ifndef __SMB_NETFS__
#define __SMB_NETFS__

#include <CoreFoundation/CoreFoundation.h>

#define SMB_PREFIX "smb://"

#ifdef __cplusplus
extern "C" {
#endif
	
netfsError SMB_CreateSessionRef(void **sessionRef);
netfsError SMB_GetServerInfo(CFURLRef url, void *sessionRef, CFDictionaryRef openOptions, CFDictionaryRef *serverParms);
netfsError SMB_ParseURL(CFURLRef url, CFDictionaryRef *urlParms);
netfsError SMB_CreateURL(CFDictionaryRef urlParms, CFURLRef *url);
netfsError SMB_OpenSession(CFURLRef url, void *sessionRef, CFDictionaryRef openOptions, CFDictionaryRef *sessionInfo);
netfsError SMB_EnumerateShares(void *sessionRef, CFDictionaryRef in_EnumerateOptions, CFDictionaryRef *sharePoints); 
netfsError SMB_Mount(void *sessionRef, CFURLRef url, CFStringRef mPoint, CFDictionaryRef mOptions, CFDictionaryRef *mInfo);
netfsError SMB_Cancel(void *sessionRef);
netfsError SMB_CloseSession(void *sessionRef);
netfsError SMB_GetMountInfo(CFStringRef in_Mountpath, CFDictionaryRef *out_MountInfo);
	
#ifdef __cplusplus
};
#endif

#endif /* __SMB_NETFS__ */

