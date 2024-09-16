/*
 * Copyright (c) 2009 - 2023 Apple Inc. All rights reserved.
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

#include <sys/types.h>
#include <os/availability.h>

#if !defined(SMBCLIENT_EXPORT)
#if defined(__GNUC__)
#define SMBCLIENT_EXPORT __attribute__((visibility("default")))
#else
#define SMBCLIENT_EXPORT
#endif
#endif /* SMBCLIENT_EXPORT */

#if !defined(_NTSTATUS_DEFINED)
#define _NTSTATUS_DEFINED
typedef u_int32_t NTSTATUS;
#endif

#if !defined(_SMBHANDLE_DEFINED)
#define _SMBHANDLE_DEFINED
struct smb_server_handle;
typedef struct smb_server_handle * SMBHANDLE;
#endif

#if !defined(_SMBFID)
#define _SMBFID
typedef u_int64_t SMBFID;
#endif

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
#define SMBFS_MNT_VALIDATE_NEG_OFF  0x0800
#define SMBFS_MNT_SUBMOUNTS_OFF     0x1000
#define SMBFS_MNT_DIR_LEASE_OFF     0x2000
#define SMBFS_MNT_FILE_DEF_CLOSE_OFF 0x4000
#define SMBFS_MNT_DIR_CACHE_OFF     0x8000
#define SMBFS_MNT_HIGH_FIDELITY     0x10000
#define SMBFS_MNT_DATACACHE_OFF     0x20000
#define SMBFS_MNT_MDATACACHE_OFF    0x40000
#define SMBFS_MNT_MULTI_CHANNEL_ON  0x80000
#define SMBFS_MNT_SNAPSHOT          0x100000
#define SMBFS_MNT_MC_PREFER_WIRED   0x200000
#define SMBFS_MNT_DISABLE_311       0x400000
#define SMBFS_MNT_SESSION_ENCRYPT   0x800000
#define SMBFS_MNT_SHARE_ENCRYPT             0x1000000
#define SMBFS_MNT_ASSUME_DUR_LEASE_V2_OFF   0x2000000
#define SMBFS_MNT_FILE_MODE                 0x4000000
#define SMBFS_MNT_DIR_MODE                  0x8000000
#define SMBFS_MNT_NO_PASSWORD_PROMPT        0x10000000
#define SMBFS_MNT_FORCE_NEW_SESSION         0x20000000
#define SMBFS_MNT_DUR_HANDLE_LOCKFID_ONLY   0x40000000
#define SMBFS_MNT_HIFI_DISABLED             0x80000000
#define SMBFS_MNT_COMPRESSION_CHAINING_OFF  0x100000000
#define SMBFS_MNT_MC_CLIENT_RSS_FORCE_ON    0x200000000

/* Compression algorithm bitmap */
#define SMB2_COMPRESSION_LZNT1_ENABLED          0x00000001
#define SMB2_COMPRESSION_LZ77_ENABLED           0x00000002
#define SMB2_COMPRESSION_LZ77_HUFFMAN_ENABLED   0x00000004
#define SMB2_COMPRESSION_PATTERN_V1_ENABLED     0x00000008

/*
 * Async Read/Write defaults
 * Minimum credits is 64 credits. Default of 2 MB will use just 32 credits
 *
 * Note that smb_tcpsndbuf/smb_tcprcvbuf will limit the max quantum size that
 * can be used.
 */

/* Anything bigger than this is broken up into quantum sizes */
#define kMaxSingleIO (1024 * 1024)

#define kSmallReadQuantumSize (256 * 1024)
#define kReadMediumQuantumSize (512 * 1024)
#define kLargeReadQuantumSize (1024  * 1024)

#define kSmallWriteQuantumSize (256 * 1024)
#define kWriteMediumQuantumSize (512 * 1024)
#define kLargeWriteQuantumSize (1024 * 1024)

#define kQuantumMaxNumber 8     /* Used with kSmallQuantumSize */
#define kQuantumMedNumber 8     /* Used with kMediumQuantumSize */
#define kQuantumMinNumber 8     /* Used with kLargeQuantumSize */

#define kSmallMTUMaxNumber 16   /* If no large MTU, then max number of requests */
#define kQuantumNumberLimit 255

#define kQuantumRecheckTimeOut 60

#define kDefaultMaxIOSize (8 * 1024 * 1024)

/* TimeMachinePB commands */
enum {
	kReadSettings = 0x01,
	kWriteSettings = 0x02
};

/* TimeMachinePB Attributes (can only be read) */
enum {
	kTMLockStealingSupported = 0x01,		/* AFP only */
	kServerReplyCacheSupported = 0x02,		/* AFP only */
	kSMBServer = 0x04,						/* SMB - Server is using SMB protocol */
	kSMBDurableHandleV2Supported = 0x08,	/* SMB - Durable Handle V2 supported */
	kSMBFullFSyncSupported = 0x10,			/* SMB - F_FULLFSYNC supported */
    kSMBLockFIDOnly = 0x20                  /* SMB - Durable handle only on O_EXLOCK/O_SHLOCK */
};

struct TimeMachinePB
{
	u_int32_t   command;			/* IN */
	u_int32_t   bitmap;			/* future use */
	u_int32_t   attributes;			/* OUT */
	u_int32_t   reconnectTimeOut;		/* IN / OUT <SMB Durable Handle Timeout> */
	u_int32_t   reconnectConnectTimeOut;	/* IN / OUT <SMB unused> */
	u_int32_t   disablePrimaryReconnect;	/* IN / OUT */
	u_int32_t   disableSecondaryReconnect;	/* IN / OUT <SMB unused> */
	u_int32_t   IP_QoS;			/* IN / OUT (0 = none) Set values like IPTOS_LOWDELAY | IPTOS_THROUGHPUT */
	u_int32_t   data[4];			/* future use */
};

#ifndef KERNEL
#include <os/log.h>
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
 * @function SMBMountShareEx
 * @abstract Mount a SMB share
 * @param inConnection A handle to the connection
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
		void (*)(void *, void *),
		void *args)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;
	
/*!
 * @function SMBMountShareExDict
 * @abstract Mount a SMB share
 * @param inConnection A handle to the connection
 * @param targetShare A UTF-8 encoded share name, may be null.
 * @param mountPoint A UTF-8 encoded mount point that must exist.
 * @param mountOptions CFDictonary of mount options
 * @param retMountInfo Returned CFDictionary containing information about the mount
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBMountShareExDict(
        SMBHANDLE       inConnection,
        const char      *targetShare,
        const char      *mountPoint,
        CFDictionaryRef mountOptions,
        CFDictionaryRef *retMountInfo,
        void (*)(void *, void *),
        void *args)
API_AVAILABLE(macos(11.3))
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
 * @inConnection - A handle to the connection
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
 * @inputBuffer - Internal only should never be looked at.
 * @inputBufferLen - Size of the inputBuffer.
 */
SMBCLIENT_EXPORT
void
SMBRemountServer(
		const void *inputBuffer,
		size_t inputBufferLen)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;
	
/*!
 * @function SMBGetDfsReferral
 * @abstract Private routine for resolving a dfs referral for smbutil
 * @url - dfs referral to resolve
 * @dfsReferralDict - dfs resolution results
 */
SMBCLIENT_EXPORT
int SMBGetDfsReferral(
		const char * url, 
		CFMutableDictionaryRef dfsReferralDict);

/*!
 * @function SMBQueryDir
 * @abstract Private routine for enumerating a directory, only SMB 2 or later
 * @inConnection - A handle to the connection
 * @file_info_class - file info class that describes return data format
 * @flags - controls how the query dir should be done
 * @file_index - starting byte offset within the dir
 * @fid - dir File ID to do query dir on
 * @name - search pattern
 * @name_len - length of the search pattern
 * @rcv_output_buffer - buffer to return Query Dir results in
 * @rcv_max_output_len - size of the rcv_output_buffer
 * @rcv_output_len - actual number of bytes returned in rcv_output_buffer
 * @query_dir_reply_len - number of bytes returned in Query Dir reply
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBQueryDir(
    SMBHANDLE       inConnection,
    uint8_t         file_info_class,
    uint8_t         flags,
    uint32_t        file_index,
    SMBFID          fid,
    const char *    name,
    uint32_t        name_len,
    char *          rcv_output_buffer,
    uint32_t        rcv_max_output_len,
    uint32_t *      rcv_output_len,
    uint32_t *      query_dir_reply_len)
__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_NA)
;

SMBCLIENT_EXPORT
time_t
SMBConvertGMT(
              const char *gmt_string)
API_AVAILABLE(macos(11.3))
;

#endif // KERNEL

	
#ifdef __cplusplus
} // extern "C"
#endif

#endif // _SMBCLIENT_INTERNAL_H_
