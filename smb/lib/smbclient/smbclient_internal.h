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

#ifndef _SMBCLIENT_INTERNAL_H_
#define _SMBCLIENT_INTERNAL_H_

/*
 * This is private to SMB Client Project code and is not for external use.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Optional smbfs mount flags */
#define SMBFS_MNT_SOFT              0x0001
#define SMBFS_MNT_NOTIFY_OFF        0x0002
#define SMBFS_MNT_STREAMS_ON        0x0004
#define SMBFS_MNT_DEBUG_ACL_ON      0x0008
#define SMBFS_MNT_DFS_SHARE         0x0010
#define SMBFS_MNT_COMPOUND_ON       0x0020
#define SMBFS_MNT_TIME_MACHINE      0x0040
#define SMBFS_MNT_READDIRATTR_OFF   0x0080
#define SMBFS_MNT_KERBEROS_OFF      0x0100  /* tmp until <12991970> is fixed */
#define SMBFS_MNT_FILE_IDS_OFF      0x0200
#define SMBFS_MNT_AAPL_OFF          0x0400

#ifndef KERNEL
#include <asl.h>	
#include <sys/mount.h>
#include <CoreFoundation/CoreFoundation.h>

/* Once we add more we may want to make this an enum, also may want to make public */
#define kHasNtwrkSID	0x01
#define kLanmanOn		0x02

/* These must match the values in dfs.h  */
#define kReferralList			CFSTR("ReferralList")
#define kRequestFileName		CFSTR("RequestFileName")
#define	kDFSPath                CFSTR("DFSPath")
#define kServerType             CFSTR("ServerType")
#define	kNetworkAddress			CFSTR("NetworkAddress")
#define kNewReferral			CFSTR("NewReferral")
#define kDfsServerArray 	    CFSTR("DfsServerArray")
#define kDfsADServerArray       CFSTR("DfsADServerArray")
#define kDfsReferralArray		CFSTR("DfsReferralArray")
#define kSpecialName			CFSTR("SpecialName")
#define kNumberOfExpandedNames  CFSTR("NumberOfExpandedNames")
#define kExpandedNameArray		CFSTR("ExpandedNameArray")

/*!
 * @function SMBLogInfo
 * @abstract Helper routine for logging information the same as the framework.
 * @printf style routine
 */	
SMBCLIENT_EXPORT
void 
SMBLogInfo( const char *, int,...) __printflike(1, 3)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

	
/*!
 * @function SMBMountShare
 * @abstract Mount a SMB share
 * @param inConnection A SMBHANDLE created by SMBOpenServer.
 * @param targetShare A UTF-8 encoded share name, may be null.
 * @param mountPoint A UTF-8 encoded mount point that must exist.
 * @param mountFlags See man mount.
 * @param mountOptions Set of options that support the alternative mount flags
 * @param fileMode Specify permissions that should be assigned to files. The 
 * value must be specified as octal numbers. To use the default value set to
 * zero.
 * @param dirMode Specify permissions that should be assigned to directories. 
 * The value must be specified as octal numbers. To use the default value set to
 * zero.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBMountShareEx(
		SMBHANDLE	inConnection,
		const char	*targetShare,
		const char	*mountPoint,
		unsigned	mountFlags,
		uint64_t	mountOptions,
		mode_t 		fileMode,
		mode_t 		dirMode,
		void (*)(void  *, void *), 
		void *args)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;
	
/*!
 * @function SMBAllocateAndSetContext
 * @abstract Creates a SMBHANDLE that contains the smb library internal session 
 * context passed into it. The handle can be used to access other SMBClient
 * Framework routines.
 * @param session - A smb library internal session context
 * @result Returns an SMBHANDLE that can be used to access the session or NULL
 */
SMBCLIENT_EXPORT 
SMBHANDLE 
SMBAllocateAndSetContext(
		void * session)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;
	
/*!
 * @function SMBCheckForAlreadyMountedShare
 * @abstract Private routine for getting information about a list of shares  
 * @inConnection - A handle to the connection
 * @shareRef - the share in question
 * @mdictRef - a dictionary to add the share to
 * @fs - List of file systems
 * @fs_cnt - Number of file systems
 * @result Returns an errno on failure and zero on success
 */
SMBCLIENT_EXPORT
int 
SMBCheckForAlreadyMountedShare(
		SMBHANDLE inConnection,
		CFStringRef shareRef, 
		CFMutableDictionaryRef mdictRef,
		struct statfs *fs, 
		int fs_cnt)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBSetNetworkIdentity
 * @abstract Private routine for getting identity information of a users 
 * connection.
 * @inConnection - A handle to the users connection
 * @network_sid - On success the users network sid
 * @account - On success the users account name
 * @domain - On success the domain the user belongs to
 * @result Returns an errno on failure and zero on success
 */
SMBCLIENT_EXPORT
int 
SMBSetNetworkIdentity(
		SMBHANDLE inConnection, 
		void *network_sid, 
		char *account, 
		char *domain)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;
	
/*!
 * @function SMBRemountServer
 * @abstract Private routine for remouting Dfs 
 * @param inputBuffer Internal only should never be looked at.
 * @param inputBufferLen Size of the inputBuffer.
 */
SMBCLIENT_EXPORT
void
SMBRemountServer(
		const void *inputBuffer,
		size_t inputBufferLen)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;
	
SMBCLIENT_EXPORT
int SMBGetDfsReferral(
		const char * url, 
		CFMutableDictionaryRef dfsReferralDict);

#endif // KERNEL

	
#ifdef __cplusplus
} // extern "C"
#endif

#endif // _SMBCLIENT_INTERNAL_H_
