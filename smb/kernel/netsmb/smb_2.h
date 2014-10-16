/*
 * Copyright (c) 2011 - 2012 Apple Inc. All rights reserved.
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

#ifndef _NETSMB_SMB_2_H_
#define _NETSMB_SMB_2_H_

#include <stdint.h>
#include <sys/types.h>

/*
 * SMB 2/3 Crediting constants
 * kCREDIT_REQUEST_AMT - number of credits to request when client needs more
 * kCREDIT_LOW_WATER - If client gets below this number of credits,
 *                     1) Start using only 1 credit at a time instead of multi
 *                     2) If multi credit leaves less than this amount, adjust
 *                        length of the request to use less credits
 *                     3) Optional Request more credits if change source code
 * kCREDIT_MIN_AMT - If client gets to this number of credits, stop sending
 *                   Be very careful in changing this value as there are times
 *                   when we only have one credit granted to us.
 * kCREDIT_MAX_AMT - maximum of credits that client will try to get.
 *                   Currently as long as below this amount, keep asking for
 *                   more credits until we get this amount.
 */
#define kCREDIT_REQUEST_AMT 256
#define kCREDIT_LOW_WATER 10
#define kCREDIT_MIN_AMT 1
/* crediting fields are UInt32, but SMB 2/3 Header has UInt16 credit fields */
#define kCREDIT_MAX_AMT UINT16_MAX

/* smb2_durable_handle flags */
typedef enum _SMB2_DURABLE_HANDLE_FLAGS
{
    SMB2_DURABLE_HANDLE_REQUEST = 0x0001,
    SMB2_DURABLE_HANDLE_RECONNECT = 0x0002,
    SMB2_DURABLE_HANDLE_GRANTED = 0x0004,
    SMB2_LEASE_GRANTED = 0x0008
} _SMB2_DURABLE_HANDLE_FLAGS;

struct smb2_durable_handle {
    uint64_t fid;               /* SMBFID to reconnect in durable handle reconnect */
    uint64_t flags;
    uint64_t lease_key_hi;      /* atomic increment number */
    uint64_t lease_key_low;     /* node hash value */
    uint32_t lease_state;
    uint32_t pad;
};

/* 
 * Apple SMB 2/3 "AAPL" Create Context extensions
 */

/* Define "AAPL" Context Command Codes */
enum {
    kAAPL_SERVER_QUERY = 1,
    kAAPL_RESOLVE_ID = 2
};

/* 
 * Server Query Request
 *
 *      uint32_t command_code = kAAPL_SERVER_QUERY;
 *      uint32_t reserved = 0;
 *      uint64_t request_bitmap;
 *      uint64_t client_capabilities;
 *
 * Server Query Response
 *
 *      uint32_t command_code = kAAPL_SERVER_QUERY;
 *      uint32_t reserved = 0;
 *      uint64_t reply_bitmap;
 *      <reply specific data>
 *
 *      The reply data is packed in the response block in the order specified
 *      by the reply_bitmap.
 *
 * Server Query request/reply bitmap
 *  Bit 0 - kAAPL_SERVER_CAPS returns uint64_t bitmap of server capabilities
 *  Bit 1 - kAAPL_VOLUME_CAPS returns uint64_t bitmap of volume capabilities
 *  Bit 2 - kAAPL_MODEL_INFO returns uint32_t Pad2 followed by uint32_t length
 *      followed by the Unicode model string. The Unicode string is padded with
 *      zeros to end on an 8 byte boundary.
 *
 * Example Server Query Context Response Buffer:
 *      uint32_t Next = 0;
 *      uint16_t NameOffset = 16;
 *      uint16_t NameLength = 4;
 *      uint16_t Reserved = 0;
 *      uint16_t DataOffset = 24;
 *      uint32_t DataLength = variable based on ModelString length;
 *      uint32_t ContextName = "AAPL";
 *      uint32_t Pad = 0;
 *      uint32_t CommandCode = kAAPL_SERVER_QUERY
 *      uint32_t Reserved = 0;
 *      uint64_t ReplyBitmap = kAAPL_SERVER_CAPS | kAAPL_VOLUME_CAPS | 
 *                             kAAPL_MODEL_INFO;
 *      uint64_t ServerCaps = kAAPL_SUPPORTS_READDIR_ATTR |
 *                            kAAPL_SUPPORTS_OSX_COPYFILE;
 *      uint64_t VolumeCaps = kAAPL_SUPPORT_RESOLVE_ID | kAAPL_CASE_SENSITIVE;
 *      uint32_t Pad2 = 0;
 *      uint32_t ModelStringLen = variable;
 *      char *   ModelString;
 *      char     PadBytes = variable to end on 8 byte boundary;
 *
 * kAAPL_SUPPORTS_NFS_ACE - Uses to set Posix permission when ACLs are off
 *      on the server. The server must allow the client to get the current
 *      ACL and then the client will return it with the desired Posix 
 *      permissions in the NFS ACE in the ACL.
 */

/* Define Server Query request/response bitmap */
enum {
    kAAPL_SERVER_CAPS = 0x01,
    kAAPL_VOLUME_CAPS = 0x02,
    kAAPL_MODEL_INFO = 0x04
};

/* Define Client/Server Capabilities bitmap */
enum {
    kAAPL_SUPPORTS_READ_DIR_ATTR = 0x01,
    kAAPL_SUPPORTS_OSX_COPYFILE = 0x02,
    kAAPL_UNIX_BASED = 0x04,
    kAAPL_SUPPORTS_NFS_ACE = 0x08
};

/* Define Volume Capabilities bitmap */
enum {
    kAAPL_SUPPORT_RESOLVE_ID = 0x01,
    kAAPL_CASE_SENSITIVE = 0x02
};

/*
 * Resolve ID Request
 *
 *      uint32_t command_code = kAAPL_RESOLVE_ID;
 *      uint32_t reserved = 0;
 *      uint64_t file_id;
 *
 * Resolve ID Response
 *
 *      uint32_t command_code = kAAPL_RESOLVE_ID;
 *      uint32_t reserved = 0;
 *      uint32_t resolve_id_ntstatus;
 *      uint32_t path_string_len = variable;
 *      char *   path_string;
 *
 * Example Resolve ID Context Response Buffer:
 *      uint32_t Next = 0;
 *      uint16_t NameOffset = 16;
 *      uint16_t NameLength = 4;
 *      uint16_t Reserved = 0;
 *      uint16_t DataOffset = 24;
 *      uint32_t DataLength = variable based on PathString length;
 *      uint32_t ContextName = "AAPL";
 *      uint32_t Pad = 0;
 *      uint32_t CommandCode = kAAPL_RESOLVE_ID;
 *      uint32_t Reserved = 0;
 *      uint32_t ResolveID_NTStatus = 0;
 *      uint32_t ServerPathLen = variable;
 *      char *   ServerPath;
 *      char     PadBytes = variable to end on 8 byte boundary;
 */

/*
 * ReadDirAttr Support
 *
 * Server has to support AAPL Create Context and support the 
 * command of kAAPL_SERVER_QUERY. In the ReplyBitMap, kAAPL_SERVER_CAPS 
 * has to be set and in the ServerCaps field, kAAPL_SUPPORTS_READ_DIR_ATTR
 * must be set.
 * 
 * Client uses FILE_ID_BOTH_DIR_INFORMATION for QueryDir
 *
 * In the Server reply for FILE_ID_BOTH_DIR_INFORMATION, fields are defined as:
 *      uint32_t ea_size;
 *      uint8_t short_name_len;
 *      uint8_t reserved;
 *      uint8_t short_name[24];
 *      uint16_t reserved2;
 *
 * If kAAPL_SUPPORTS_READ_DIR_ATTR is set, the fields will be filled in as:
 *      uint32_t max_access;
 *      uint8_t short_name_len = 0;
 *      uint8_t reserved = 0;
 *      uint64_t rsrc_fork_len;
 *      uint8_t compressed_finder_info[16];
 *      uint16_t unix_mode;  (only if kAAPL_UNIX_BASED is set)
 *
 * Notes:
 *      (1) ea_size is the max access if SMB_EFA_REPARSE_POINT is NOT set in
 *      the file attributes. For a reparse point, the SMB Client will assume 
 *      full access.
 *      (2) short_name is now the Resource Fork logical length and minimal
 *      Finder Info.  
 *      (3) SMB Cient will calculate the resource fork allocation size based on
 *      block size. This will be done in all places resource fork allocation
 *      size is returned by the SMB Client so we return consistent answers.
 *      (4) Compressed Finder Info will be only the fields actually still in
 *      use in the regular Finder Info and in the Ext Finder Info. SMB client
 *      will build a normal Finder Info and Ext Finder Info and fill in the 
 *      other fields in with zeros.
 *      (5) If kAAPL_UNIX_BASED is set, then reserved2 is the entire Posix mode
 *
 *          struct smb_finder_file_info {
 *              uint32_t finder_type;
 *              uint32_t finder_creator;
 *              uint16_t finder_flags;
 *              uint16_t finder_ext_flags;
 *              uint32_t finder_date_added;
 *          }
 *
 *          struct smb_finder_folder_info {
 *              uint64_t reserved1;
 *              uint16_t finder_flags;
 *              uint16_t finder_ext_flags;
 *              uint32_t finder_date_added;
 *          }
 *
 *
 * Normal Finder Info and Extended Finder Info definitions
 *          struct finder_file_info {
 *              uint32_t finder_type;
 *              uint32_t finder_creator;
 *              uint16_t finder_flags;
 *              uint32_t finder_old_location = 0;
 *              uint16_t reserved = 0;
 *
 *              uint32_t reserved2 = 0;
 *              uint32_t finder_date_added;
 *              uint16_t finder_ext_flags;
 *              uint16_t reserved3 = 0;
 *              uint32_t reserved4 = 0;
 *          }
 *
 *          struct finder_folder_info {
 *              uint64_t reserved1;
 *              uint16_t finder_flags;
 *              uint32_t finder_old_location = 0;
 *              uint16_t finder_old_view_flags = 0;
 *
 *              uint32_t finder_old_scroll_position = 0;
 *              uint32_t finder_date_added;
 *              uint16_t finder_ext_flags;
 *              uint16_t reserved3 = 0;
 *              uint32_t reserved4 = 0;
 *          }
 */

struct smb_finder_file_info {
    uint32_t finder_type;
    uint32_t finder_creator;
    uint16_t finder_flags;
    uint32_t finder_date_added;
    uint16_t finder_ext_flags;
};

struct smb_finder_folder_info {
    uint64_t reserved1;
    uint16_t finder_flags;
    uint32_t finder_date_added;
    uint16_t finder_ext_flags;
};

struct smb_finder_info {
    union {
        struct smb_finder_file_info file_info;
        struct smb_finder_folder_info folder_info;
    } u2;
};

struct finder_file_info {
    uint32_t finder_type;
    uint32_t finder_creator;
    uint16_t finder_flags;
    uint32_t finder_old_location;   /* always set to 0 */
    uint16_t reserved;              /* always set to 0 */
    /* End of Finder Info and start of Ext Finder Info */
    uint32_t reserved2;             /* always set to 0 */
    uint32_t finder_date_added;
    uint16_t finder_ext_flags;
    uint16_t reserved3;             /* always set to 0 */
    uint32_t reserved4;             /* always set to 0 */
};

struct finder_folder_info {
    uint64_t reserved1;
    uint16_t finder_flags;
    uint32_t finder_old_location;   /* always set to 0 */
    uint16_t finder_old_view_flags; /* always set to 0 */
    /* End of Finder Info and start of Ext Finder Info */
    uint32_t finder_old_scroll_position; /* always set to 0 */
    uint32_t finder_date_added;
    uint16_t finder_ext_flags;
    uint16_t reserved3;             /* always set to 0 */
    uint32_t reserved4;             /* always set to 0 */
};

struct finder_info {
    union {
        struct finder_file_info file_info;
        struct finder_folder_info folder_info;
    } u2;
};


/* SMB 2/3 Commands, 2.2.1 */
#define SMB2_NEGOTIATE		0x0000
#define SMB2_SESSION_SETUP	0x0001
#define SMB2_LOGOFF		0x0002
#define SMB2_TREE_CONNECT	0x0003
#define SMB2_TREE_DISCONNECT	0x0004
#define SMB2_CREATE		0x0005
#define SMB2_CLOSE		0x0006
#define SMB2_FLUSH		0x0007
#define SMB2_READ		0x0008
#define SMB2_WRITE		0x0009
#define SMB2_LOCK		0x000A
#define SMB2_IOCTL		0x000B
#define SMB2_CANCEL		0x000C
#define SMB2_ECHO		0x000D
#define SMB2_QUERY_DIRECTORY	0x000E
#define SMB2_CHANGE_NOTIFY	0x000F
#define SMB2_QUERY_INFO		0x0010
#define SMB2_SET_INFO		0x0011
#define SMB2_OPLOCK_BREAK	0x0012

/* SMB 2/3 Write Request Header Length, 2.2.21 */
#define SMB2_WRITE_REQ_HDRLEN       48

/* SMB 2/3 Dialects, 2.2.3 */
#define SMB2_DIALECT_0202   0x0202
#define SMB2_DIALECT_02ff   0x02ff
#define SMB2_DIALECT_0210   0x0210
#define SMB2_DIALECT_0300   0x0300
#define SMB2_DIALECT_0302   0x0302

#define	SMB2_TID_UNKNOWN	0xffffffff

/* Bitmask to define the valid SMB command range. */
#define SMB2_VALID_COMMAND_MASK 0x001F

/* SMB 2/3 Flags, 2.2.1 */
#define SMB2_FLAGS_SERVER_TO_REDIR      0x00000001
#define SMB2_FLAGS_ASYNC_COMMAND        0x00000002
#define SMB2_FLAGS_RELATED_OPERATIONS   0x00000004
#define SMB2_FLAGS_SIGNED               0x00000008
#define SMB2_FLAGS_DFS_OPERATIONS       0x10000000

/* Bitmask to define the valid SMB flags set. */
#define SMB2_VALID_FLAGS_MASK 0x1000000F

/* SMB 2/3 Security Mode, 2.2.3 */
#define SMB2_NEGOTIATE_SIGNING_ENABLED	0x0001
#define SMB2_NEGOTIATE_SIGNING_REQUIRED	0x0002

/* SMB 2/3 Negotiate Capabilities, 2.2.3 */
#define SMB2_GLOBAL_CAP_DFS                 0x00000001
#define SMB2_GLOBAL_CAP_LEASING             0x00000002
#define SMB2_GLOBAL_CAP_LARGE_MTU           0x00000004
#define SMB2_GLOBAL_CAP_MULTI_CHANNEL       0x00000008
#define SMB2_GLOBAL_CAP_PERSISTENT_HANDLES	0x00000010
#define SMB2_GLOBAL_CAP_DIRECTORY_LEASING	0x00000020
#define SMB2_GLOBAL_CAP_ENCRYPTION          0x00000040

/* SMB 2/3 SessionFlags, 2.2.6 */
#define SMB2_SESSION_FLAG_IS_GUEST      0x0001
#define SMB2_SESSION_FLAG_IS_NULL       0x0002
#define SMB2_SESSION_FLAG_ENCRYPT_DATA  0x0004  /* Encryption Required */

/* SMB 2/3 ShareType, 2.2.10 */
#define SMB2_SHARE_TYPE_DISK	0x01
#define SMB2_SHARE_TYPE_PIPE	0x02
#define SMB2_SHARE_TYPE_PRINT	0x03

/* SMB 2/3 ShareFlags, 2.2.10 */
#define SMB2_SHAREFLAG_DFS              0x00000001
#define SMB2_SHAREFLAG_DFS_ROOT         0x00000002
#define SMB2_SHAREFLAG_ENCRYPT_DATA     0x00008000 /* Encryption Required */

/* SMB 2/3 ShareCapabilities, 2.2.10 */
#define SMB2_SHARE_CAP_DFS                      0x00000008
#define SMB2_SHARE_CAP_CONTINUOUS_AVAILABILITY  0x00000010

/* SMB 2/3 RequestedOplockLevel, 2.2.13 */
#define SMB2_OPLOCK_LEVEL_NONE	    0x00
#define SMB2_OPLOCK_LEVEL_II	    0x01
#define SMB2_OPLOCK_LEVEL_EXCLUSIVE 0x08
#define SMB2_OPLOCK_LEVEL_BATCH	    0x09
#define SMB2_OPLOCK_LEVEL_LEASE	    0xff

/* SMB 2/3 Lease Break Notification Flags, 2.2.23.2 */
#define SMB2_NOTIFY_BREAK_LEASE_FLAG_ACK_REQUIRED   0x01

/* SMB 2/3 RequestedLeaseLevel, 2.2.13 */
#define SMB2_LEASE_NONE             0x00
#define SMB2_LEASE_READ_CACHING	    0x01
#define SMB2_LEASE_HANDLE_CACHING   0x02
#define SMB2_LEASE_WRITE_CACHING	0x04

/* SMB 2/3 ImpersonationLevel, 2.2.13 */
#define SMB2_IMPERSONATION_ANONYMOUS	    0x00000000
#define SMB2_IMPERSONATION_IDENTIFICATION   0x00000001
#define SMB2_IMPERSONATION_IMPERSONATION    0x00000002
#define SMB2_IMPERSONATION_DELEGATE	    0x00000003

#define SMB2_WRITEFLAG_WRITE_THROUGH	0x00000001


/*
 * Access mask encoding:
 *
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | | | | | | | | | | |1| | | | | | | | | |2| | | | | | | | | |3| |
 * |0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|0|1|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |R|W|E|A|   |M|S|  standard     |  specific                     |
 * +-------+-------+---------------+-------------------------------+
 *
 * R => generic read
 * W => generic write
 * E => generic execute
 * A => generic all
 * S => SACL access (ACCESS_SYSTEM_SECURITY)
 * M => maximal access
 */

#define SMB2_GENERIC_ALL		0x10000000
#define SMB2_GENERIC_EXECUTE		0x20000000
#define SMB2_GENERIC_WRITE		0x40000000
#define SMB2_GENERIC_READ		0x80000000

/* #define SMB2_FILE_LIST_DIRECTORY	0x00000001 */   /* defined in smb.h */
/* #define SMB2_FILE_ADD_FILE		0x00000002 */   /* defined in smb.h */
/* #define SMB2_FILE_ADD_SUBDIRECTORY	0x00000004 */ /* defined in smb.h */
#define SMB2_FILE_READ_EA		0x00000008
#define SMB2_FILE_WRITE_EA		0x00000010
/* #define SMB2_FILE_TRAVERSE		0x00000020 */   /* defined in smb.h */
#define SMB2_FILE_DELETE_CHILD		0x00000040
#define SMB2_FILE_READ_ATTRIBUTES	0x00000080
#define SMB2_FILE_WRITE_ATTRIBUTES	0x00000100

#define SMB2_STD_ACCESS_DELETE		0x00010000
#define SMB2_STD_ACCESS_READ_CONTROL	0x00020000
#define SMB2_STD_ACCESS_WRITE_DAC	0x00040000
#define SMB2_STD_ACCESS_WRITE_OWNER	0x00080000
#define SMB2_STD_ACCESS_SYNCHRONIZE	0x00100000
#define SMB2_STD_ACCESS_SYSTEM_SECURITY	0x01000000
#define SMB2_STD_ACCESS_MAXIMAL		0x02000000
#define SMB2_STD_RESERVED_1		0x04000000
#define SMB2_STD_RESERVED_2		0x08000000

/* SMB 2/3 CREATE_CONTEXT names, 2.2.13.2 */
#define SMB2_CREATE_EA_BUFFER                   0x45787441 /* "ExtA" */
#define SMB2_CREATE_SD_BUFFER                   0x53656344 /* "SecD" */
#define SMB2_CREATE_DURABLE_HANDLE_REQUEST      0x44486e51 /* "DHnQ" */
#define SMB2_CREATE_DURABLE_HANDLE_RECONNECT    0x44486e43 /* "DHnC" */
#define SMB2_CREATE_ALLOCATION_SIZE             0x416c5369 /* "AISi" */
#define SMB2_CREATE_QUERY_MAXIMAL_ACCESS        0x4d784163 /* "MxAc" */
#define SMB2_CREATE_TIMEWARP_TOKEN              0x54577270 /* "Twrp" */
#define SMB2_CREATE_QUERY_ON_DISK_ID            0x51466964 /* "QFid" */
#define SMB2_CREATE_REQUEST_LEASE               0x52714c73 /* "RqLs" */

/* Apple Defined Contexts */
#define	SMB2_CREATE_AAPL                        0x4141504c


/* SMB 2/3 CloseFlags, 2.2.15 */
#define SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB    0x0001

/* SMB 2/3 Lockflags, 2.2.26.1 */
#define SMB2_LOCKFLAG_SHARED_LOCK	0x00000001
#define SMB2_LOCKFLAG_EXCLUSIVE_LOCK	0x00000002
#define SMB2_LOCKFLAG_UNLOCK		0x00000004
#define SMB2_LOCKFLAG_FAIL_IMMEDIATELY	0x00000010

/* SMB 2/3 IoctlFlags, 2.2.31 */
#define SMB2_IOCTL_IS_FSCTL		0x00000001

/* SMB 2/3 QUERY_DIRECTORY Flags, 2.2.33 */
#define SMB2_RESTART_SCANS		0x01
#define SMB2_RETURN_SINGLE_ENTRY	0x02
#define SMB2_INDEX_SPECIFIED		0x04
#define SMB2_REOPEN			0x10

/* SMB 2/3 CHANGE_NOTIFY Flags, 2.2.35 */
#define SMB2_WATCH_TREE			0x0001

/* SMB 2/3 QUERY_INFO InfoType, 2.2.37 */
#define SMB2_0_INFO_FILE	0x01
#define SMB2_0_INFO_FILESYSTEM	0x02
#define SMB2_0_INFO_SECURITY	0x03
#define SMB2_0_INFO_QUOTA	0x04

/* MS-FSCC 2.5 FileSystem Information Classes.
 * Also see MSDN for ZwQueryVolumeInformationFile.
 */
typedef enum _FS_INFORMATION_CLASS
{
    FileFsVolumeInformation     = 1, /* Query */
    FileFsLabelInformation      = 2, /* Set */
    FileFsSizeInformation       = 3, /* Query */
    FileFsDeviceInformation     = 4, /* Query */
    FileFsAttributeInformation  = 5, /* Query */
    FileFsControlInformation    = 6, /* Query, Set */
    FileFsFullSizeInformation   = 7, /* Query */
    FileFsObjectIdInformation   = 8, /* Query, Set */
    FileFsDriverPathInformation = 9 /* Query */
} FS_INFORMATION_CLASS;

/* MS-FSCC 2.4 File Information Classes */
typedef enum _FILE_INFORMATION_CLASS
{
    FileDirectoryInformation        = 1,
    FileFullDirectoryInformation    = 2,
    FileBothDirectoryInformation    = 3,
    FileBasicInformation            = 4,
    FileStandardInformation         = 5,
    FileInternalInformation         = 6,
    FileEaInformation               = 7,
    FileAccessInformation           = 8,
    FileNameInformation             = 9,
    FileRenameInformation           = 10,
    FileLinkInformation             = 11,
    FileNamesInformation            = 12,
    FileDispositionInformation      = 13,
    FilePositionInformation         = 14,
    FileFullEaInformation           = 15,
    FileModeInformation             = 16,
    FileAlignmentInformation        = 17,
    FileAllInformation              = 18,
    FileAllocationInformation       = 19,
    FileEndOfFileInformation        = 20,
    FileAlternateNameInformation    = 21,
    FileStreamInformation           = 22,
    FilePipeInformation             = 23,
    FilePipeLocalInformation        = 24,
    FilePipeRemoteInformation       = 25,
    FileMailslotQueryInformation    = 26,
    FileMailslotSetInformation      = 27,
    FileCompressionInformation      = 28,
    FileObjectIdInformation         = 29,
    FileMoveClusterInformation      = 31,
    FileQuotaInformation            = 32,
    FileReparsePointInformation     = 33,
    FileNetworkOpenInformation      = 34,
    FileAttributeTagInformation     = 35,
    FileTrackingInformation         = 36,
    FileIdBothDirectoryInformation  = 37,
    FileIdFullDirectoryInformation  = 38,
    FileValidDataLengthInformation  = 39,
    FileShortNameInformation        = 40,
    FileSfioReserveInformation      = 44,
    FileSfioVolumeInformation       = 45,
    FileHardLinkInformation         = 46,
    FileNormalizedNameInformation   = 48,
    FileIdGlobalTxDirectoryInformation = 50,
    FileStandardLinkInformation     = 54
} FILE_INFORMATION_CLASS;

#if !defined(_SMBFID)
#define _SMBFID
typedef uint64_t SMBFID;
#endif

#if !defined(_SMB2FID)
#define _SMB2FID
typedef struct _SMB2FID
{
    uint64_t fid_persistent;
    uint64_t fid_volatile;
} SMB2FID;
#endif

/* 
 * BasicInfo 40 bytes, StdInfo 24 bytes, InternalInfo 8 bytes,
 * EaInfo 4 bytes, AccessInfo 4 bytes, PosInfo 8 bytes, ModeInfo 4 bytes,
 * AlignInfo 4 bytes, Name Info 4 bytes + name length (PATH_MAX)
 * which adds up to be 100 + PATH_MAX
 */
#define SMB2_FILE_ALL_INFO_LEN (100 + PATH_MAX)

struct FILE_ALL_INFORMATION
{
    struct smb_share *share;
    struct smbnode *np;
    struct smbfattr *fap;
    const char **namep;
    size_t *name_lenp;
};

struct FILE_FS_ATTRIBUTE_INFORMATION
{
    uint32_t file_system_attrs;
    uint32_t max_component_name_len;
    uint32_t file_system_name_len;
    uint32_t pad;
    char *file_system_namep;
};

struct FILE_FS_SIZE_INFORMATION
{
    uint64_t total_alloc_units;
    uint64_t avail_alloc_units;
    uint32_t sectors_per_alloc_unit;
    uint32_t bytes_per_sector;
};

/* FILE_STREAM_INFORMATION flags */
typedef enum _FILE_STREAM_INFO_FLAGS
{
    SMB_NO_RESOURCE_FORK = 0x0001,
    SMB_NO_FINDER_INFO = 0x0002,
    SMB_NO_TRANSLATE_NAMES = 0x0010,  /* input flag-  Don't translate stream names to xattr names */
    SMB_NO_SUBSTREAMS = 0x0020
} _FILE_STREAM_INFO_FLAGS;

struct FILE_STREAM_INFORMATION
{
    struct smb_share *share;
    struct smbnode *np;
    const char *namep;
    size_t name_len;
    void *uio;
    size_t *stream_buf_sizep;
    const char *stream_namep;
    uint64_t *stream_sizep;
    uint64_t *stream_alloc_sizep;
    uint32_t *stream_flagsp;
};

/* SMB3 Encryption defines */

/* Authenticated Data */
#define SMB3_AES_AUTHDATA_OFF       20
#define SMB3_AES_AUTHDATA_LEN       32
#define SMB3_CCM_NONCE_LEN          11

/* Transform Header (TF) */
#define SMB3_AES_TF_HDR_LEN         52

#define SMB2_ENCRYPTION_AES128_CCM  0x0001

#define SMB3_AES_TF_PROTO_OFF   0
#define	SMB3_AES_TF_PROTO_STR   "\xFDSMB"
#define	SMB3_AES_TF_PROTO_LEN   4

#define SMB3_AES_TF_SIG_OFF     4
#define SMB3_AES_TF_SIG_LEN     16

#define SMB3_AES_TF_NONCE_OFF   20
#define SMB3_AES_TF_NONCE_LEN   16

#define SMB3_AES_TF_MSGLEN_OFF  36
#define SMB3_AES_TF_MSGLEN_LEN  4

#define SMB3_AES_TF_ENCR_ALG_OFF    42
#define SMB3_AES_TF_ENCR_ALG_LEN    2

#define SMB3_AES_TF_SESSID_OFF      44
#define SMB3_AES_TF_SESSID_LEN      8

/* SMB 3 Transform Header */

struct smb3_aes_transform_hdr
{
    uint32_t        proto;
    unsigned char   signature[SMB3_AES_TF_SIG_LEN];
    unsigned char   nonce[SMB3_AES_TF_NONCE_LEN];
    uint32_t        orig_msg_size;
    uint16_t        reserved;
    uint16_t        encrypt_algorithm;
    uint64_t        sess_id;
} __attribute__((__packed__));

typedef struct smb3_aes_transform_hdr SMB3_AES_TF_HEADER;

#endif /* SMB_SMB2_H */
