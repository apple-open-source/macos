/*
 * Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
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

#ifndef _NETFS_SMBCLIENT_H_
#define _NETFS_SMBCLIENT_H_

/*
 * These are private routines that should only be used by the internal 
 * smbfs project.
 */

#include <CoreFoundation/CoreFoundation.h>


#ifdef __cplusplus
extern "C" {
#endif
	
/*!
 * @function SMBNetFsLockSession
 * @abstract Locks the session from other threads, except cancel.
 * @param inConnection SMBHANDLE to lock
 */	
SMBCLIENT_EXPORT
void 
SMBNetFsLockSession(
					SMBHANDLE inConnection);
	
/*!
 * @function SMBNetFsUnlockSession
 * @abstract unlocks the session.
 * @param inConnection SMBHANDLE to unlock
 */	
SMBCLIENT_EXPORT
void 
SMBNetFsUnlockSession(
					SMBHANDLE inConnection);
	
/*!
 * @function SMBNetFsCreateSessionRef
 * @abstract Create a session reference that can be used by all other routines.
 * @param sessionRef The session to return, null if failure
 * @result A netfs error, see NetFS framework.
 */	
SMBCLIENT_EXPORT
int32_t 
SMBNetFsCreateSessionRef(
		SMBHANDLE *outConnection);
	
/*!
 * @function SMBNetFsCancel
 * @abstract Cancel any outstanding call to this session.
 * @param sessionRef The session returned from SMBNetFsCreateSessionRef
 * @result A netfs error, see NetFS framework.
 */	
SMBCLIENT_EXPORT
int32_t 
SMBNetFsCancel(
	SMBHANDLE inConnection);

/*!
 * @function SMBNetFsCloseSession
 * @abstract Close this session and free all memory.
 * @param sessionRef The session returned from SMBNetFsCreateSessionRef
 * @result A netfs error, see NetFS framework.
 */	
SMBCLIENT_EXPORT
int32_t 
SMBNetFsCloseSession(
	SMBHANDLE inConnection);

/*!
 * @function SMBNetFsParseURL
 * @abstract Parse the url into a dictionary.
 * @param url The smb url to parse
 * @param urlParms A dictionary that contains all the parts of the url.
 * @result A netfs error, see NetFS framework.
 */	
SMBCLIENT_EXPORT
int32_t 
SMBNetFsParseURL(
	CFURLRef url, 
	CFDictionaryRef *urlParms);

/*!
 * @function SMBNetFsCreateURL
 * @abstract Create a url based on the dictionary
 * @param urlParms A dictionary that contains all the parts of the url.
 * @param url The smb url create from the dictionary
 * @result A netfs error, see NetFS framework.
 */	
SMBCLIENT_EXPORT
int32_t 
SMBNetFsCreateURL(
	CFDictionaryRef urlParms, 
	CFURLRef *url);

/*!
 * @function SMBNetFsOpenSession
 * @abstract Open a connection to the server and obtain authentication.
 * @param url The url to use for the connection
 * @param sessionRef The session returned from SMBNetFsCreateSessionRef
 * @param openOptions The options used for the connection and authentication.
 * @param sessionInfo Infromation about the connection and authentication used.
 * @result A netfs error, see NetFS framework.
 */	
SMBCLIENT_EXPORT
int32_t 
SMBNetFsOpenSession(
	CFURLRef url, 
	SMBHANDLE inConnection, 
	CFDictionaryRef openOptions, 
	CFDictionaryRef *sessionInfo);

/*!
 * @function SMBNetFsGetServerInfo
 * @abstract Open a connection to the server.
 * @param url The url to use for the connection
 * @param sessionRef The session returned from SMBNetFsCreateSessionRef
 * @param openOptions The options used for the connection.
 * @param sessionInfo Infromation about the connected server.
 * @result A netfs error, see NetFS framework.
 */	
SMBCLIENT_EXPORT
int32_t 
SMBNetFsGetServerInfo(
	CFURLRef url, 
	SMBHANDLE inConnection, 
	CFDictionaryRef openOptions, 
	CFDictionaryRef *serverParms);
	
/*!
 * @function SMBNetFsTreeConnectForEnumerateShares
 * @abstract Open a IPC$ tree connection to the server.
 * @param sessionRef The session returned from SMBNetFsCreateSessionRef
 * @result A netfs error, see NetFS framework.
 */	
SMBCLIENT_EXPORT
int32_t 
SMBNetFsTreeConnectForEnumerateShares(
	SMBHANDLE inConnection);

/*!
 * @function SMBNetFsMount
 * @abstract Mount a volume using the session and url provided.
 * @param sessionRef The session returned from SMBNetFsCreateSessionRef
 * @param url The url to use for the mount
 * @param mPoint Where to mount the volume.
 * @param mOptions The options used for the mount.
 * @param mInfo Infromation about the mount.
 * @result A netfs error, see NetFS framework.
 */	
SMBCLIENT_EXPORT
int32_t 
SMBNetFsMount(
	SMBHANDLE inConnection, 
	CFURLRef url, 
	CFStringRef mPoint, 
	CFDictionaryRef mOptions, 
	CFDictionaryRef *mInfo,
	void (*callout)(void  *, void *), 
	void *args);

/*!
 * @function SMBNetFsGetMountInfo
 * @abstract Retrieve information about a mount point.
 * @param in_Mountpath Path to volume that the requested information is needed.
 * @param out_MountInfo The information requested.
 * @result A netfs error, see NetFS framework.
 */	
SMBCLIENT_EXPORT
int32_t 
SMBNetFsGetMountInfo(
	CFStringRef in_Mountpath, 
	CFDictionaryRef *out_MountInfo);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _NETFS_SMBCLIENT_H_
