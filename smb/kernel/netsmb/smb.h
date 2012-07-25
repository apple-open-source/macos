/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
 * 
 * Now many of these defines are from samba4 code, by Andrew Tridgell.
 * (Permission given to Conrad Minshall at CIFS plugfest Aug 13 2003.)
 * (Note the main decision was whether to use defines found in MS includes
 * and web pages, versus Samba, and the deciding factor is which developers
 * are more likely to be looking at this code base.)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Common definintions and structures for SMB/CIFS protocol
 */
 
#ifndef _NETSMB_SMB_H_
#define _NETSMB_SMB_H_

#define	NBNS_UDP_PORT_137	137
#define	NBSS_TCP_PORT_139	139
#define	SMB_TCP_PORT_445	445

/*
 * Formats of data/string buffers
 */
#define	SMB_DT_DATA		1
#define	SMB_DT_DIALECT		2
#define	SMB_DT_PATHNAME		3
#define	SMB_DT_ASCII		4
#define	SMB_DT_VARIABLE		5

/* We require a mux of ten to support remote notifcations */
#define SMB_NOTIFY_MIN_MUX	10
/*
 * SMB header
 */
#define	SMB_SIGNATURE			"\xFFSMB"
#define	SMB_SIGLEN				4
#define	SMB_HDRCMD(p)			(*((u_char*)(p) + SMB_SIGLEN))
#define	SMB_HDRPIDHIGH(p)		(letohs(*(uint16_t*)((u_char*)(p) + 12)))
#define	SMB_HDRTID(p)			(letohs(*(uint16_t*)((u_char*)(p) + 24)))
#define	SMB_HDRPIDLOW(p)		(letohs(*(uint16_t*)((u_char*)(p) + 26)))
#define	SMB_HDRUID(p)			(letohs(*(uint16_t*)((u_char*)(p) + 28)))
#define	SMB_HDRMID(p)			(letohs(*(uint16_t*)((u_char*)(p) + 30)))
#define	SMB_HDRLEN				32
#define	SMB_WRITEANDX_HDRLEN	32
#define	SMB_READANDX_HDRLEN		30
#define SMB_MAX_SETUPCOUNT_LEN	255
#define SMB_COM_NT_TRANS_LEN	48 

/*
 * bits in the smb_flags field
 */
#define SMB_FLAGS_SUPPORT_LOCKREAD      0x01
#define SMB_FLAGS_CLIENT_BUF_AVAIL      0x02
#define	SMB_FLAGS_CASELESS		0x08
#define SMB_FLAGS_CANONICAL_PATHNAMES	0x10
#define SMB_FLAGS_REQUEST_OPLOCK        0x20
#define SMB_FLAGS_REQUEST_BATCH_OPLOCK  0x40
#define SMB_FLAGS_SERVER_RESP		0x80

/*
 * bits in the smb_flags2 field
 */
#define	SMB_FLAGS2_KNOWS_LONG_NAMES	0x0001
#define	SMB_FLAGS2_KNOWS_EAS		0x0002	/* client know about EAs */
#define	SMB_FLAGS2_SECURITY_SIGNATURE	0x0004	/* check SMB integrity */
#define	SMB_FLAGS2_IS_LONG_NAME		0x0040	/* any path name is long name */
#define	SMB_FLAGS2_EXT_SEC		0x0800	/* client aware of Extended
						 * Security negotiation */
#define	SMB_FLAGS2_DFS			0x1000	/* resolve paths in DFS */
#define	SMB_FLAGS2_PAGING_IO		0x2000	/* for exec */
#define	SMB_FLAGS2_ERR_STATUS		0x4000	/* 1 - status.status */
#define	SMB_FLAGS2_UNICODE		0x8000	/* use Unicode for all strings */

#define	SMB_UID_UNKNOWN		0xffff
#define	SMB_TID_UNKNOWN		0xffff

/*
 * Security mode bits
 */
#define SMB_SM_USER		0x01	/* server in the user security mode */
#define	SMB_SM_ENCRYPT	0x02	/* use challenge/responce */
#define	SMB_SM_SIGS		0x04
#define	SMB_SM_SIGS_REQ 0x08

/*
 * Action bits in session setup reply
 */
#define SMB_ACT_GUEST		0x01

/*
 * NTLM capabilities
 */
#define	SMB_CAP_RAW_MODE		0x0001
#define	SMB_CAP_MPX_MODE		0x0002
#define	SMB_CAP_UNICODE			0x0004
#define	SMB_CAP_LARGE_FILES		0x0008	/* 64 bit offsets supported */
#define	SMB_CAP_NT_SMBS			0x0010
#define	SMB_CAP_RPC_REMOTE_APIS		0x0020
#define	SMB_CAP_STATUS32		0x0040
#define	SMB_CAP_LEVEL_II_OPLOCKS	0x0080
#define	SMB_CAP_LOCK_AND_READ		0x0100
#define	SMB_CAP_NT_FIND			0x0200
#define	SMB_CAP_DFS			0x1000
#define	SMB_CAP_INFOLEVEL_PASSTHRU	0x2000
#define	SMB_CAP_LARGE_READX		0x4000
#define	SMB_CAP_LARGE_WRITEX		0x8000
#define	SMB_CAP_UNIX			0x00800000
#define	SMB_CAP_BULK_TRANSFER		0x20000000
#define	SMB_CAP_COMPRESSED_DATA		0x40000000
#define	SMB_CAP_EXT_SECURITY		0x80000000
/* Used for checking to see if we are connecting to a NT4 server */
#define SMB_CAP_LARGE_RDWRX	(SMB_CAP_LARGE_WRITEX | SMB_CAP_LARGE_READX)

/* UNIX CAPS */
#define CIFS_UNIX_MAJOR_VERSION 1
#define CIFS_UNIX_MINOR_VERSION 0

/* UNIX PROTOCOL EXTENSIONS CAP */
#define CIFS_UNIX_FCNTL_LOCKS_CAP           	0x1
#define CIFS_UNIX_POSIX_ACLS_CAP            	0x2
#define CIFS_UNIX_XATTTR_CAP	            	0x4 /* for support of other xattr namespaces such as system, security and trusted */
#define CIFS_UNIX_EXTATTR_CAP					0x8 /* for support of chattr (chflags) and lsattr */
#define CIFS_UNIX_POSIX_PATHNAMES_CAP			0x10 /* Use POSIX pathnames on the wire. */
#define CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP		0x20 /* Support new info */

/* 
 * INTERNAL UNIX EXTENSIONS CAP
 *
 * Define which unix call we can make to the server
 */
#define UNIX_QFS_UNIX_INFO_CAP				0x01
#define UNIX_QFS_POSIX_WHOAMI_CAP			0x02
#define UNIX_QFS_POSIX_WHOAMI_SID_CAP		0x04
#define UNIX_QFILEINFO_UNIX_LINK_CAP		0x08
#define UNIX_SFILEINFO_UNIX_LINK_CAP		0x10
#define UNIX_QFILEINFO_UNIX_INFO2_CAP		0x20
#define UNIX_FIND_FILE_UNIX_INFO2_CAP		UNIX_QFILEINFO_UNIX_INFO2_CAP
#define UNIX_SFILEINFO_UNIX_INFO2_CAP		0x40
#define UNIX_SFILEINFO_POSIX_UNLINK_CAP		0x80

/* Use by the client to say we are using posix names, not sure about are client */
#define SMB_QUERY_POSIX_FS_INFO     0x201


/* SMB_COM_TREE_CONNECT_ANDX reply word count */
#define TREE_CONNECT_NORMAL_WDCNT		3
#define TREE_CONNECT_EXTENDED_WDCNT		7

/* SMB_COM_TREE_CONNECT_ANDX  flags. See [MS-SMB] for a complete description. */
#define TREE_CONNECT_ANDX_DISCONNECT_TID		0x0001
#define TREE_CONNECT_ANDX_EXTENDED_SIGNATURES	0x0004
#define TREE_CONNECT_ANDX_EXTENDED_RESPONSE		0x0008

/*
 * SMB_COM_TREE_CONNECT_ANDX  optional support flags. See [MS-SMB] for a complete
 * description.
 */
#define SMB_SUPPORT_SEARCH_BITS		0x0001	/* Server supports SearchAttributes */
#define SMB_SHARE_IS_IN_DFS			0x0002	/* This share is managed by DFS */
#define SMB_CSC_MASK				0x000C	/* Offline-caching bits for this share. */
#define	SMB_UNIQUE_FILE_NAME		0x0010	/* Long file names only */
#define SMB_EXTENDED_SIGNATURES		0x0020	/* Signing key protection is on. */
/* See [MS-SMB] for a complete description of SMB_CSC_MASK bits. */
#define SMB_CSC_CACHE_MANUAL_REINT	0x0000
#define SMB_CSC_CACHE_AUTO_REINT	0x0004
#define SMB_CSC_CACHE_VDO			0x0008

/*
 * Resource Types 
 */
#define kFileTypeDisk				0x0000
#define kFileTypeByteModePipe		0x0001
#define kFileTypeMessageModePipe	0x0002
#define kFileTypePrinter			0x0003
#define kFileTypeUnknown			0xffff

/*
 * If the ResourceType field is FileTypeDisk, then this field MUST be the 
 * FileStatusFlags field:
 *
 * FileStatusFlags (2 bytes): A 16-bit field that shows extra information about 
 * the opened file or directory. Any combination of the following flags is valid. 
 * Unused bit fields SHOULD be set to zero by the server and MUST be ignored by the client.
 */
#define kNO_EAS			0x0001	/* The file or directory has no extended attributes. */
#define kNO_SUBSTREAMS	0x0002	/* The file or directory has no data streams other than the main data stream. */
#define kNO_REPARSETAG	0x0004	/* The file or directory is not a reparse point. */

/*
 * Extended file attributes
 */
#define SMB_EFA_RDONLY          0x0001
#define SMB_EFA_HIDDEN          0x0002
#define SMB_EFA_SYSTEM          0x0004
#define SMB_EFA_VOLUME          0x0008
#define SMB_EFA_DIRECTORY       0x0010
#define SMB_EFA_ARCHIVE         0x0020 
#define SMB_EFA_DEVICE          0x0040
#define SMB_EFA_NORMAL          0x0080
#define SMB_EFA_TEMPORARY       0x0100
#define SMB_EFA_SPARSE          0x0200
#define SMB_EFA_REPARSE_POINT   0x0400 
#define SMB_EFA_COMPRESSED      0x0800
#define SMB_EFA_OFFLINE         0x1000
#define SMB_EFA_NONINDEXED      0x2000
#define SMB_EFA_ENCRYPTED       0x4000
#define SMB_EFA_POSIX_SEMANTICS 0x01000000
#define SMB_EFA_BACKUP_SEMANTICS 0x02000000
#define SMB_EFA_DELETE_ON_CLOSE 0x04000000
#define SMB_EFA_SEQUENTIAL_SCAN 0x08000000
#define SMB_EFA_RANDOM_ACCESS   0x10000000
#define SMB_EFA_NO_BUFFERING    0x20000000
#define SMB_EFA_WRITE_THROUGH   0x80000000


/*
 * Access Mode Encoding
 */
#define	SMB_AM_OPENREAD		0x0000
#define	SMB_AM_OPENWRITE	0x0001
#define	SMB_AM_OPENRW		0x0002
#define	SMB_AM_OPENEXEC		0x0003
#define	SMB_AM_OPENMODE		0x0003	/* mask for access mode bits */
#define	SMB_SM_COMPAT		0x0000
#define	SMB_SM_EXCLUSIVE	0x0010
#define	SMB_SM_DENYWRITE	0x0020
#define	SMB_SM_DENYREADEXEC	0x0030
#define	SMB_SM_DENYNONE		0x0040

/* NT_CREATE_ANDX reply word count */
#define NTCREATEX_NORMAL_WDCNT		34
#define NTCREATEX_EXTENDED_WDCNT	42
#define NTCREATEX_BRKEN_SPEC_26_WDCNT	26

/* NT_CREATE_ANDX flags */
#define NTCREATEX_FLAGS_REQUEST_OPLOCK          0x02
#define NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK    0x04
#define NTCREATEX_FLAGS_OPEN_DIRECTORY          0x08
#define NTCREATEX_FLAGS_EXTENDED                0x10

/* NT_CREATE_ANDX share_access (share mode) */
#define NTCREATEX_SHARE_ACCESS_NONE     0
#define NTCREATEX_SHARE_ACCESS_READ     1
#define NTCREATEX_SHARE_ACCESS_WRITE    2
#define NTCREATEX_SHARE_ACCESS_DELETE   4
#define NTCREATEX_SHARE_ACCESS_ALL      7

/*
 * CreateDisposition 
 * Specifies the action to perform if the file does or does not exist. 
 * CreateDisposition can be one of the values in the following table.
 *
 * CreateDisposition value	Action if file exists			Action if file does not exist
 * FILE_SUPERSEDE			Replace the file.					Create the file.
 * FILE_OPEN				Open the file.						Return an error.
 * FILE_CREATE				Return an error.					Create the file.
 * FILE_OPEN_IF				Open the file.						Create the file.
 * FILE_OVERWRITE			Open the file, and overwrite it.	Return an error.
 * FILE_OVERWRITE_IF		Open the file, and overwrite it.	Create the file.
 *
 * See [MS-CIFS].pdf for complete description
 */
#define FILE_SUPERSEDE        0	
#define FILE_OPEN             1
#define FILE_CREATE           2
#define FILE_OPEN_IF          3
#define FILE_OVERWRITE        4
#define FILE_OVERWRITE_IF     5

/* NT_CREATE_ANDX create_options */     
#define NTCREATEX_OPTIONS_DIRECTORY             0x0001
#define NTCREATEX_OPTIONS_WRITE_THROUGH         0x0002
#define NTCREATEX_OPTIONS_SEQUENTIAL_ONLY       0x0004
#define NTCREATEX_OPTIONS_SYNC_ALERT            0x0010
#define NTCREATEX_OPTIONS_ASYNC_ALERT           0x0020
#define NTCREATEX_OPTIONS_NON_DIRECTORY_FILE    0x0040
#define NTCREATEX_OPTIONS_NO_EA_KNOWLEDGE       0x0200
#define NTCREATEX_OPTIONS_EIGHT_DOT_THREE_ONLY  0x0400
#define NTCREATEX_OPTIONS_RANDOM_ACCESS         0x0800
#define NTCREATEX_OPTIONS_DELETE_ON_CLOSE       0x1000
#define NTCREATEX_OPTIONS_OPEN_BY_FILE_ID		0x2000
/* 
 * If the CreateOptions parameter specifies the FILE_OPEN_REPARSE_POINT flag and 
 * NtCreateFile opens a file with a reparse point, normal reparse processing does 
 * not occur and NtCreateFile attempts to directly open the reparse point file. 
 * If the FILE_OPEN_REPARSE_POINT flag is not specified, normal reparse point 
 * processing occurs for the file. In either case, if the open operation was 
 * successful, NtCreateFile returns STATUS_SUCCESS; otherwise, an error code. 
 * The NtCreateFile function never returns STATUS_REPARSE, if FILE_OPEN_REPARSE_POINT
 * is set.
 */
#define NTCREATEX_OPTIONS_OPEN_REPARSE_POINT	0x00200000

/* NT_CREATE_ANDX "impersonation" */
#define NTCREATEX_IMPERSONATION_ANONYMOUS       0
#define NTCREATEX_IMPERSONATION_IDENTIFICATION  1
#define NTCREATEX_IMPERSONATION_IMPERSONATION   2
#define NTCREATEX_IMPERSONATION_DELEGATION      3

/* NT_CREATE_ANDX security flags */
#define NTCREATEX_SECURITY_DYNAMIC      1       
#define NTCREATEX_SECURITY_ALL          2       

/* SMB_TRANS2_FIND_FIRST2/SMB_TRANS2_FIND_NEXT2 flags */
#define FIND2_CLOSE_AFTER_REQUEST	0x0001
#define FIND2_CLOSE_ON_EOS		0x0002
#define FIND2_RETURN_RESUME_KEYS	0x0004
#define FIND2_CONTINUE_SEARCH		0x0008
#define FIND2_BACKUP_INTENT		0x0010

/*
 * SMB commands
 */
#define	SMB_COM_CREATE_DIRECTORY        0x00
#define	SMB_COM_DELETE_DIRECTORY        0x01
#define	SMB_COM_OPEN                    0x02
#define	SMB_COM_CREATE                  0x03
#define	SMB_COM_CLOSE                   0x04
#define	SMB_COM_FLUSH                   0x05
#define	SMB_COM_DELETE                  0x06
#define	SMB_COM_RENAME                  0x07
#define	SMB_COM_QUERY_INFORMATION       0x08
#define	SMB_COM_SET_INFORMATION         0x09
#define	SMB_COM_READ                    0x0A
#define	SMB_COM_WRITE                   0x0B
#define	SMB_COM_LOCK_BYTE_RANGE         0x0C
#define	SMB_COM_UNLOCK_BYTE_RANGE       0x0D
#define	SMB_COM_CREATE_TEMPORARY        0x0E
#define	SMB_COM_CREATE_NEW              0x0F
#define	SMB_COM_CHECK_DIRECTORY         0x10
#define	SMB_COM_PROCESS_EXIT            0x11
#define	SMB_COM_SEEK                    0x12
#define	SMB_COM_LOCK_AND_READ           0x13
#define	SMB_COM_WRITE_AND_UNLOCK        0x14
#define	SMB_COM_READ_RAW                0x1A
#define	SMB_COM_READ_MPX                0x1B
#define	SMB_COM_READ_MPX_SECONDARY      0x1C
#define	SMB_COM_WRITE_RAW               0x1D
#define	SMB_COM_WRITE_MPX               0x1E
#define	SMB_COM_WRITE_COMPLETE          0x20
#define	SMB_COM_SET_INFORMATION2        0x22
#define	SMB_COM_QUERY_INFORMATION2      0x23
#define	SMB_COM_LOCKING_ANDX            0x24
#define	SMB_COM_TRANSACTION             0x25
#define	SMB_COM_TRANSACTION_SECONDARY   0x26
#define	SMB_COM_IOCTL                   0x27
#define	SMB_COM_IOCTL_SECONDARY         0x28
#define	SMB_COM_COPY                    0x29
#define	SMB_COM_MOVE                    0x2A
#define	SMB_COM_ECHO                    0x2B
#define	SMB_COM_WRITE_AND_CLOSE         0x2C
#define	SMB_COM_OPEN_ANDX               0x2D
#define	SMB_COM_READ_ANDX               0x2E
#define	SMB_COM_WRITE_ANDX              0x2F
#define	SMB_COM_CLOSE_AND_TREE_DISC     0x31
#define	SMB_COM_TRANSACTION2            0x32
#define	SMB_COM_TRANSACTION2_SECONDARY  0x33
#define	SMB_COM_FIND_CLOSE2             0x34
#define	SMB_COM_FIND_NOTIFY_CLOSE       0x35
#define	SMB_COM_TREE_CONNECT			0x70
#define	SMB_COM_TREE_DISCONNECT         0x71
#define	SMB_COM_NEGOTIATE               0x72
#define	SMB_COM_SESSION_SETUP_ANDX      0x73
#define	SMB_COM_LOGOFF_ANDX             0x74
#define	SMB_COM_TREE_CONNECT_ANDX       0x75
#define	SMB_COM_QUERY_INFORMATION_DISK  0x80
#define	SMB_COM_SEARCH                  0x81
#define	SMB_COM_FIND                    0x82
#define	SMB_COM_FIND_UNIQUE             0x83
#define	SMB_COM_NT_TRANSACT             0xA0
#define	SMB_COM_NT_TRANSACT_SECONDARY   0xA1
#define	SMB_COM_NT_CREATE_ANDX          0xA2
#define	SMB_COM_NT_CANCEL               0xA4
#define	SMB_COM_OPEN_PRINT_FILE         0xC0
#define	SMB_COM_WRITE_PRINT_FILE        0xC1
#define	SMB_COM_CLOSE_PRINT_FILE        0xC2
#define	SMB_COM_GET_PRINT_QUEUE         0xC3
#define	SMB_COM_READ_BULK               0xD8
#define	SMB_COM_WRITE_BULK              0xD9
#define	SMB_COM_WRITE_BULK_DATA         0xDA

/*
 * SMB_COM_TRANSACTION2 subcommands
 */
#define	SMB_TRANS2_OPEN2			0x00
#define	SMB_TRANS2_FIND_FIRST2			0x01
#define	SMB_TRANS2_FIND_NEXT2			0x02
#define	SMB_TRANS2_QUERY_FS_INFORMATION		0x03
#define SMB_TRANS2_SETFSINFO                    0x04
#define	SMB_TRANS2_QUERY_PATH_INFORMATION	0x05
#define	SMB_TRANS2_SET_PATH_INFORMATION		0x06
#define	SMB_TRANS2_QUERY_FILE_INFORMATION	0x07
#define	SMB_TRANS2_SET_FILE_INFORMATION		0x08
#define	SMB_TRANS2_FSCTL			0x09
#define	SMB_TRANS2_IOCTL2			0x0A
#define	SMB_TRANS2_FIND_NOTIFY_FIRST		0x0B
#define	SMB_TRANS2_FIND_NOTIFY_NEXT		0x0C
#define	SMB_TRANS2_CREATE_DIRECTORY		0x0D
#define	SMB_TRANS2_SESSION_SETUP		0x0E
#define	SMB_TRANS2_GET_DFS_REFERRAL		0x10
#define	SMB_TRANS2_REPORT_DFS_INCONSISTENCY	0x11

/*
 * SMB_COM_NT_TRANSACT subcommands
 */
#define NT_TRANSACT_CREATE		0x01
#define NT_TRANSACT_IOCTL		0x02
#define NT_TRANSACT_SET_SECURITY_DESC	0x03
#define NT_TRANSACT_NOTIFY_CHANGE	0x04
#define NT_TRANSACT_RENAME		0x05
#define NT_TRANSACT_QUERY_SECURITY_DESC	0x06
#define NT_TRANSACT_GET_USER_QUOTA	0x07
#define NT_TRANSACT_SET_USER_QUOTA	0x08

/*
 * SMB_TRANS2_QUERY_FS_INFORMATION levels
 */
#define SMB_QFS_ALLOCATION              1
#define SMB_QFS_VOLUME                  2
#define SMB_QFS_LABEL_INFO		0x101
#define SMB_QFS_VOLUME_INFO             0x102
#define SMB_QFS_SIZE_INFO               0x103
#define SMB_QFS_DEVICE_INFO             0x104
#define SMB_QFS_ATTRIBUTE_INFO          0x105
#define SMB_QFS_UNIX_INFO               0x200
#define SMB_QFS_POSIX_WHOAMI     	0x202
#define SMB_QFS_MAC_FS_INFO             0x301
#define SMB_QFS_VOLUME_INFORMATION      1001
#define SMB_QFS_SIZE_INFORMATION        1003
#define SMB_QFS_DEVICE_INFORMATION      1004
#define SMB_QFS_ATTRIBUTE_INFORMATION   1005
#define SMB_QFS_QUOTA_INFORMATION       1006
#define SMB_QFS_FULL_SIZE_INFORMATION   1007
#define SMB_QFS_OBJECTID_INFORMATION    1008

/*
 * NT Notify Change Compeletion Filter
*/
#define FILE_NOTIFY_CHANGE_FILE_NAME	0x00000001
#define FILE_NOTIFY_CHANGE_DIR_NAME		0x00000002
#define FILE_NOTIFY_CHANGE_ATTRIBUTES	0x00000004
#define FILE_NOTIFY_CHANGE_SIZE			0x00000008
#define FILE_NOTIFY_CHANGE_LAST_WRITE	0x00000010
#define FILE_NOTIFY_CHANGE_LAST_ACCESS	0x00000020
#define FILE_NOTIFY_CHANGE_CREATION		0x00000040
#define FILE_NOTIFY_CHANGE_EA			0x00000080
#define FILE_NOTIFY_CHANGE_SECURITY		0x00000100
#define FILE_NOTIFY_CHANGE_STREAM_NAME	0x00000200
#define FILE_NOTIFY_CHANGE_STREAM_SIZE	0x00000400
#define FILE_NOTIFY_CHANGE_STREAM_WRITE	0x00000800

/*
 * NT Notify Actions
 */
#define FILE_ACTION_ADDED				0x00000001
#define FILE_ACTION_REMOVED				0x00000002
#define FILE_ACTION_MODIFIED			0x00000003
#define FILE_ACTION_RENAMED_OLD_NAME	0x00000004
#define FILE_ACTION_RENAMED_NEW_NAME	0x00000005
#define FILE_ACTION_ADDED_STREAM		0x00000006
#define FILE_ACTION_REMOVED_STREAM		0x00000007
#define FILE_ACTION_MODIFIED_STREAM		0x00000008

/*
 * SMB_QFS_ATTRIBUTE_INFO bits.
 */
#define FILE_CASE_SENSITIVE_SEARCH      0x00000001
#define FILE_CASE_PRESERVED_NAMES       0x00000002
#define FILE_UNICODE_ON_DISK			0x00000004
#define FILE_PERSISTENT_ACLS            0x00000008
#define FILE_FILE_COMPRESSION           0x00000010
#define FILE_VOLUME_QUOTAS              0x00000020
#define FILE_SUPPORTS_SPARSE_FILES      0x00000040
#define FILE_SUPPORTS_REPARSE_POINTS    0x00000080
#define FILE_SUPPORTS_REMOTE_STORAGE    0x00000100
#define FILE_SUPPORTS_LONG_NAMES		0x00004000
#define FILE_VOLUME_IS_COMPRESSED       0x00008000
#define FILE_SUPPORTS_OBJECT_IDS        0x00010000
#define FILE_SUPPORTS_ENCRYPTION        0x00020000
#define FILE_NAMED_STREAMS              0x00040000
#define FILE_READ_ONLY_VOLUME           0x00080000

/* 
 * Mask of which WHOAMI bits are valid. This should make it easier for clients
 * to cope with servers that have different sets of WHOAMI flags (as more get added).
 */
#define SMB_WHOAMI_MASK 0x00000001

/*
 * SMBWhoami - Query the user mapping performed by the server for the
 * connected tree. This is a subcommand of the TRANS2_QFSINFO.
 *
 * Returns:
 *          4 bytes unsigned -      mapping flags (smb_whoami_flags)
 *          4 bytes unsigned -      flags mask
 *
 *          8 bytes unsigned -      primary UID
 *          8 bytes unsigned -      primary GID
 *          4 bytes unsigned -      number of supplementary GIDs
 *          4 bytes unsigned -      number of SIDs
 *          4 bytes unsigned -      SID list byte count
 *          4 bytes -               pad / reserved (must be zero)
 *
 *          8 bytes unsigned[] -    list of GIDs (may be empty)
 *          DOM_SID[] -             list of SIDs (may be empty)
 */

/*
 * SMB_TRANS2_QUERY_PATH levels
 */
#define SMB_QFILEINFO_STANDARD                  1
#define SMB_QFILEINFO_EA_SIZE                   2
#define SMB_QFILEINFO_EAS_FROM_LIST             3
#define SMB_QFILEINFO_ALL_EAS                   4
#define SMB_QFILEINFO_IS_NAME_VALID             6       /* QPATHINFO only? */
#define SMB_QFILEINFO_BASIC_INFO                0x101   
#define SMB_QFILEINFO_STANDARD_INFO             0x102
#define SMB_QFILEINFO_EA_INFO                   0x103
#define SMB_QFILEINFO_NAME_INFO                 0x104
#define SMB_QFILEINFO_ALLOCATION_INFO			0x105
#define SMB_QFILEINFO_END_OF_FILE_INFO			0x106
#define SMB_QFILEINFO_ALL_INFO                  0x107
#define SMB_QFILEINFO_ALT_NAME_INFO             0x108
#define SMB_QFILEINFO_STREAM_INFO               0x109
#define SMB_QFILEINFO_COMPRESSION_INFO          0x10b
#define SMB_QFILEINFO_UNIX_BASIC                0x200
#define SMB_QFILEINFO_UNIX_LINK                 0x201
#define SMB_QFILEINFO_POSIX_ACL					0x204
#define SMB_QFILEINFO_UNIX_INFO2				0x20B   /* UNIX File Info*/
#define SMB_QFILEINFO_MAC_DT_GET_APPL           0x306
#define SMB_QFILEINFO_MAC_DT_GET_ICON           0x307
#define SMB_QFILEINFO_MAC_DT_GET_ICON_INFO      0x308
#define SMB_QFILEINFO_MAC_SPOTLIGHT				0x310
#define SMB_QFILEINFO_BASIC_INFORMATION         1004
#define SMB_QFILEINFO_STANDARD_INFORMATION      1005
#define SMB_QFILEINFO_INTERNAL_INFORMATION      1006
#define SMB_QFILEINFO_EA_INFORMATION            1007
#define SMB_QFILEINFO_ACCESS_INFORMATION        1008
#define SMB_QFILEINFO_NAME_INFORMATION          1009
#define SMB_QFILEINFO_POSITION_INFORMATION      1014
#define SMB_QFILEINFO_MODE_INFORMATION          1016
#define SMB_QFILEINFO_ALIGNMENT_INFORMATION     1017
#define SMB_QFILEINFO_ALL_INFORMATION           1018
#define SMB_QFILEINFO_ALT_NAME_INFORMATION      1021
#define SMB_QFILEINFO_STREAM_INFORMATION        1022
#define SMB_QFILEINFO_COMPRESSION_INFORMATION   1028
#define SMB_QFILEINFO_NETWORK_OPEN_INFORMATION  1034
#define SMB_QFILEINFO_ATTRIBUTE_TAG_INFORMATION 1035

/*
 * SMB_TRANS2_FIND_FIRST2 information levels
 */
#define SMB_FIND_STANDARD               1
#define SMB_FIND_EA_SIZE                2
#define SMB_FIND_EAS_FROM_LIST          3
#define SMB_FIND_DIRECTORY_INFO         0x101
#define SMB_FIND_FULL_DIRECTORY_INFO    0x102
#define SMB_FIND_NAME_INFO              0x103
#define SMB_FIND_BOTH_DIRECTORY_INFO    0x104
#define SMB_FIND_UNIX_INFO              0x200
/* Transact 2 Find First levels */
#define SMB_FIND_FILE_UNIX             0x202
#define SMB_FIND_FILE_UNIX_INFO2       0x20B /* UNIX File Info2 */

/*
 * These are used by findfrist/next to determine the number of max search
 * elements the client should be requesting. These values are the number of 
 * bytes each structure takes up in the packet if the associated name was empty.
 * So we divided transaction buffer size by this number and that gives us the 
 * max search count to request. In each case we counted up the number of uint32_t 
 * that each structure contained, so a uint64_t counts as two uint32_t. In both
 * cases we add 2 bytes to represent the empty UTF8 name. So SMB_FIND_BOTH_DIRECTORY_INFO
 * has 16 uint32_t fields plus 30 bytes of other data and the SMB_FIND_FILE_UNIX_INFO2 
 * has 32 uint32_t fields.
 */
#define SMB_FIND_BOTH_DIRECTORY_INFO_MIN_LEN ((4 * 16) + 30 + 2)
#define SMB_FIND_FILE_UNIX_INFO2_MIN_LEN ((4 * 32) + 2)

/*
 * SMB_QUERY_FILE_UNIX_INFO2 is SMB_QUERY_FILE_UNIX_BASIC with create
 * time and file flags appended. The corresponding info level for
 * findfirst/findnext is SMB_FIND_FILE_UNIX_UNIX2.
 *     Size    Offset  Value
 *     ---------------------
 *      0      LARGE_INTEGER EndOfFile  File size
 *      8      LARGE_INTEGER Blocks     Number of blocks used on disk
 *      16     LARGE_INTEGER ChangeTime Attribute change time
 *      24     LARGE_INTEGER LastAccessTime           Last access time
 *      32     LARGE_INTEGER LastModificationTime     Last modification time
 *      40     LARGE_INTEGER Uid        Numeric user id for the owner
 *      48     LARGE_INTEGER Gid        Numeric group id of owner
 *      56     ULONG Type               Enumeration specifying the file type
 *      60     LARGE_INTEGER devmajor   Major device number if type is device
 *      68     LARGE_INTEGER devminor   Minor device number if type is device
 *      76     LARGE_INTEGER uniqueid   This is a server-assigned unique id
 *      84     LARGE_INTEGER permissions             Standard UNIX permissions
 *      92     LARGE_INTEGER nlinks     Number of hard link)
 *      100    LARGE_INTEGER CreationTime             Create/birth time
 *      108    ULONG FileFlags          File flags enumeration
 *      112    ULONG FileFlagsMask      Mask of valid flags
 */

#define SMB_DEFAULT_NO_CHANGE	-1
#define SMB_MODE_NO_CHANGE	(uint64_t)-1
#define SMB_UID_NO_CHANGE	-1
#define SMB_GID_NO_CHANGE	-1
#define SMB_SIZE_NO_CHANGE	(uint64_t)-1
#define SMB_FLAGS_NO_CHANGE	0

/* 
 * Flags for chflags (CIFS_UNIX_EXTATTR_CAP capability) and
 * SMB_QUERY_FILE_UNIX_BASIC2 (or whatever)
 */
#define EXT_SECURE_DELETE		0x00000001
#define EXT_ENABLE_UNDELETE		0x00000002
#define EXT_SYNCHRONOUS			0x00000004
#define EXT_IMMUTABLE			0x00000008
#define EXT_OPEN_APPEND_ONLY	0x00000010
#define EXT_DO_NOT_BACKUP		0x00000020
#define EXT_NO_UPDATE_ATIME		0x00000040
#define EXT_HIDDEN				0x00000080
/* The minimum set that is required by the Mac Client */
#define EXT_REQUIRED_BY_MAC		(EXT_IMMUTABLE | EXT_HIDDEN | EXT_DO_NOT_BACKUP)

/* Still expected to only contain 12 bits (little endian): */
#define EXT_UNIX_S_ISUID    0004000   /* set UID bit */
#define EXT_UNIX_S_ISGID    0002000   /* set-group-ID bit (see below) */
#define EXT_UNIX_S_ISVTX    0001000   /* sticky bit (see below) */
#define EXT_UNIX_S_IRUSR    00400     /* owner has read permission */
#define EXT_UNIX_S_IWUSR    00200     /* owner has write permission */
#define EXT_UNIX_S_IXUSR    00100     /* owner has execute permission */
#define EXT_UNIX_S_IRGRP    00040     /* group has read permission */
#define EXT_UNIX_S_IWGRP    00020     /* group has write permission */
#define EXT_UNIX_S_IXGRP    00010     /* group has execute permission */
#define EXT_UNIX_S_IROTH    00004     /* others have read permission */
#define EXT_UNIX_S_IWOTH    00002     /* others have write permission */
#define EXT_UNIX_S_IXOTH    00001     /* others have execute permission */


/* File type is still the same enumeration (little endian) as: */
#define EXT_UNIX_FILE      0
#define EXT_UNIX_DIR       1
#define EXT_UNIX_SYMLINK   2
#define EXT_UNIX_CHARDEV   3
#define EXT_UNIX_BLOCKDEV  4
#define EXT_UNIX_FIFO      5
#define EXT_UNIX_SOCKET    6

/*
 * Selectors for NT_TRANSACT_QUERY_SECURITY_DESC and
 * NT_TRANSACT_SET_SECURITY_DESC.  Details found in the MSDN
 * library by searching on security_information.
 * Note the protected/unprotected bits did not exist in NT.
 */

#define OWNER_SECURITY_INFORMATION		0x00000001
#define GROUP_SECURITY_INFORMATION		0x00000002
#define DACL_SECURITY_INFORMATION		0x00000004
#define SACL_SECURITY_INFORMATION		0x00000008
#define UNPROTECTED_SACL_SECURITY_INFORMATION	0x10000000
#define UNPROTECTED_DACL_SECURITY_INFORMATION	0x20000000
#define PROTECTED_SACL_SECURITY_INFORMATION	0x40000000
#define PROTECTED_DACL_SECURITY_INFORMATION	0x80000000

/*
 * The SECURITY_DESCRIPTOR structure defines an object's security attributes. 
 * These attributes specify who owns the object, who can access the object and 
 * what they can do with it, what level of audit logging should be applied to 
 * the object, and what kind of restrictions apply to the use of the security 
 * descriptor.
 *
 * See [MS-DTYP].pdf for more details.
 */
struct ntsecdesc {
	uint8_t	Revision;		/* This field MUST be set to one. */
	uint8_t	Sbz1;			/* In our case this field is reserved and MUST be set to zero. */
	uint16_t	ControlFlags;	/* This specifies control access bit flags. The Self Relative bit MUST be set. */
	uint32_t	OffsetOwner;	/* offset to owner SID */
	uint32_t	OffsetGroup;	/* offset to group SID */
	uint32_t	OffsetSacl;		/* offset to system/audit ACL */
	uint32_t	OffsetDacl;		/* offset to discretionary ACL */
} __attribute__((__packed__));



/*
 * ControlFlags - Control bits
 */
#define SE_OWNER_DEFAULTED		0x0001	/* Set when the owner was established by default means. */
#define SE_GROUP_DEFAULTED		0x0002	/* Set when the group was established by default means. */
#define SE_DACL_PRESENT			0x0004	/* Set when the DACL is present on the object. */
#define SE_DACL_DEFAULTED		0x0008	/* Set when the DACL was established by default means. */
#define SE_SACL_PRESENT			0x0010	/* Set when the SACL is present on the object. */
#define SD_SACL_DEFAULTED		0x0020	/* Set when the SACL was established by default means. */
#define SE_SERVER_SECURITY		0x0040	/* 
										 * Set when the caller wants the system to create a Server ACL based 
										 * on the input ACL, regardless of its source (explicit or defaulting). 
										 */
#define SE_DACL_TRUSTED			0x0080	/* 
										 * Set when ACL pointed to by the DACL field was provided by a 
										 * trusted source and does not require any editing of compound ACEs. 
										 */
#define SE_DACL_AUTO_INHERIT_REQ 0x0100	/* Set when the DACL should be computed through inheritance. */
#define SE_SACL_AUTO_INHERIT_REQ 0x0200	/* Set when the SACL should be computed through inheritance. */
#define SE_DACL_AUTO_INHERITED	0x0400	/* Set when the DACL was created through inheritance. */
#define SE_SACL_AUTO_INHERITED	0x0800	/* Set when the SACL was created through inheritance. */
#define SE_DACL_PROTECTED		0x1000	/* Set when the DACL should be protected from inherit operations. */
#define SE_SACL_PROTECTED		0x2000	/* Set when the SACL should be protected from inherit operations. */
#define SE_RM_CONTROL_VALID		0x4000	/* 
										 * Set when the resource manager control bits are valid. For more  
										 * information about resource managers, see [MS-SECO] section 4.1. 
										 */
#define SE_SELF_RELATIVE		0x8000	/* 
										 * Set when the security descriptor is in self-relative format.  
										 * Cleared when the security descriptor is in absolute format. 
										 */

/*
 * access control list header
 * it is followed by the ACEs
 * note this is "raw", ie little-endian
 */
struct ntacl {
	uint8_t	acl_revision;	/* 0x02 observed with W2K */
	uint8_t	acl_pad1;
	uint16_t	acl_len; /* bytes; includes this header */
	uint16_t	acl_acecount;
	uint16_t	acl_pad2;
} __attribute__((__packed__));

#define acllen(a) (letohs((a)->acl_len))
#define wset_acllen(a, l) ((a)->acl_len = htoles(l))
#define wset_aclacecount(a, c) ((a)->acl_acecount = htoles(c))
#define aclace(a) ((struct ntace *)((char *)(a) + sizeof(struct ntacl)))

/*
 * access control entry header
 * it is followed by type-specific ace data,
 * which for the simple types is just a SID
 * note this is "raw", ie little-endian
 */
struct ntace {
	uint8_t	ace_type;
	uint8_t	ace_flags;
	uint16_t	ace_len; /* bytes; includes this header */
	uint32_t	ace_rights; /* generic, standard, specific, etc */
} __attribute__((__packed__));

#define acetype(a) ((a)->ace_type)
#define wset_acetype(a, t) ((a)->ace_type = (t))
#define aceflags(a) ((a)->ace_flags)
#define wset_aceflags(a, f) ((a)->ace_flags = (f))
#define acelen(a) (letohs((a)->ace_len))
#define wset_acelen(a, l) ((a)->ace_len = htoles(l))
#define acerights(a) (letohl((a)->ace_rights))
#define wset_acerights(a, r) ((a)->ace_rights = htolel(r))
#define aceace(a) ((struct ntace *)((char *)(a) + acelen(a)))
#define acesid(a) ((struct ntsid *)((char *)(a) + sizeof(struct ntace)))

/*
 * We take the Windows SMB2 define access modes and add a SMB2 in front to protect 
 * us from namespace collisions. May want to move these to a more general include
 * file in the future. Would have been nice if the kauth.h file had used the same
 * number scheme as Windows.
 */
#define SMB2_FILE_READ_DATA			0x00000001	/* Indicates the right to read data from the file, directory or named pipe. */
#define SMB2_FILE_WRITE_DATA		0x00000002	/* Indicates the right to write data into the file or named pipe beyond the end of the file. */
#define SMB2_FILE_APPEND_DATA		0x00000004	/* Indicates the right to append data into the file or named pipe. */
#define SMB2_FILE_READ_EA			0x00000008	/* Indicates the right to read the extended attributes of the file, directory or named pipe. */
#define SMB2_FILE_WRITE_EA			0x00000010	/* Indicates the right to write or change the extended attributes to the file, directory or named pipe. */
#define SMB2_FILE_EXECUTE			0x00000020	/* Indicates the right to execute the file. */
#define SMB2_FILE_DELETE_CHILD		0x00000040	/* Indicates the right to delete the files and directories within this directory. */
#define SMB2_FILE_READ_ATTRIBUTES	0x00000080	/* Indicates the right to read the attributes of the file or directory. */
#define SMB2_FILE_WRITE_ATTRIBUTES	0x00000100	/* Indicates the right to change the attributes of the file or directory. */

#define SMB2_DELETE					0x00010000	/* Indicates the right to delete the file or directory */
#define SMB2_READ_CONTROL			0x00020000	/* Indicates the right to read the security descriptor for the file, directory or named pipe. */
#define SMB2_WRITE_DAC				0x00040000	/* Indicates the right to change the discretionary access control list (DACL) in the security descriptor for the file directory or named pipe. */
#define SMB2_WRITE_OWNER			0x00080000	/* Indicates the right to change the owner in the security descriptor for the file, directory or named pipe. */
#define SMB2_SYNCHRONIZE			0x00100000	/* SMB2 clients set this flag to any value. SMB2 servers MUST ignore this flag. */

#define	SMB2_ACCESS_SYSTEM_SECURITY	0x01000000	/* Indicates the right to read or change the system access control list (SACL) in the security descriptor for the file, directory or named pipe.  */
#define	SMB2_MAXIMAL_ACCESS			0x02000000	/* Indicates that the client is requesting an open to the file with the highest level of access the client has on this file. If no access is granted for the client on this file, the server MUST fail the open with STATUS_ACCESS_DENIED. */

#define SMB2_FILE_LIST_DIRECTORY	SMB2_FILE_READ_DATA		/* Indicates the right to enumerate the contents of the directory. */
#define SMB2_FILE_ADD_FILE			SMB2_FILE_WRITE_DATA	/* Indicates the right to create a file under the directory. */
#define SMB2_FILE_ADD_SUBDIRECTORY	SMB2_FILE_APPEND_DATA	/* Indicates the right to add a sub-directory under the directory. */
#define SMB2_FILE_TRAVERSE			SMB2_FILE_EXECUTE		/* Indicates the right to traverse this directory if the server enforces traversal checking. */

#define SA_RIGHT_FILE_ALL_ACCESS	0x000001FF
#define STD_RIGHT_ALL_ACCESS		0x001F0000

#define SMB2_GENERIC_ALL			0x10000000	/* Indicates a request for all the access flags that are previously listed except MAXIMAL_ACCESS and ACCESS_SYSTEM_SECURITY. */
#define SMB2_GENERIC_EXECUTE		0x20000000	/* Indicates a request for the following combination of access flags listed above: FILE_READ_ATTRIBUTES| FILE_EXECUTE| SYNCHRONIZE| READ_CONTROL. */
#define SMB2_GENERIC_WRITE			0x40000000	/* Indicates a request for the following combination of access flags listed above: FILE_WRITE_DATA| FILE_APPEND_DATA| FILE_WRITE_ATTRIBUTES| FILE_WRITE_EA| SYNCHRONIZE| READ_CONTROL. */
#define SMB2_GENERIC_READ			0x80000000	/* Indicates a request for the following combination of access flags listed above: FILE_READ_DATA| FILE_READ_ATTRIBUTES| FILE_READ_EA| SYNCHRONIZE| READ_CONTROL. */

/*
 * This is an internal define, this value is not part of any Windows Documentation.
 * The is used to decide if the share ACL doesn't allow any type of write access.
 * In that case we set the mount point to be read only.
 */
#define FILE_FULL_WRITE_ACCESS	(SMB2_FILE_WRITE_DATA | SMB2_FILE_APPEND_DATA | \
								SMB2_FILE_WRITE_EA | SMB2_FILE_WRITE_ATTRIBUTES | \
								SMB2_DELETE | SMB2_WRITE_DAC | SMB2_WRITE_OWNER)

/*
 * security identifier header
 * it is followed by sid_numauth sub-authorities,
 * which are 32 bits each.
 * note the subauths are little-endian on the wire, but
 * need to be big-endian for memberd/DS
 */
#define SIDAUTHSIZE 6
struct ntsid {
	uint8_t	sid_revision;
	uint8_t	sid_subauthcount;
	uint8_t	sid_authority[SIDAUTHSIZE]; /* ie not little endian */
} __attribute__((__packed__));

#define sidlen(s) (sizeof(struct ntsid) + (sizeof(uint32_t) * (s)->sid_subauthcount))
#define MAXSIDLEN (sizeof(struct ntsid) + (sizeof(uint32_t) * KAUTH_NTSID_MAX_AUTHORITIES))

/*
 * MS' defined values for ace_type
 */
#define ACCESS_ALLOWED_ACE_TYPE                 0x0
#define ACCESS_DENIED_ACE_TYPE                  0x1
#define SYSTEM_AUDIT_ACE_TYPE                   0x2
#define SYSTEM_ALARM_ACE_TYPE                   0x3
#define ACCESS_ALLOWED_COMPOUND_ACE_TYPE        0x4
#define ACCESS_ALLOWED_OBJECT_ACE_TYPE          0x5
#define ACCESS_DENIED_OBJECT_ACE_TYPE           0x6
#define SYSTEM_AUDIT_OBJECT_ACE_TYPE            0x7
#define SYSTEM_ALARM_OBJECT_ACE_TYPE            0x8
#define ACCESS_ALLOWED_CALLBACK_ACE_TYPE        0x9
#define ACCESS_DENIED_CALLBACK_ACE_TYPE         0xA
#define ACCESS_ALLOWED_CALLBACK_OBJECT_ACE_TYPE 0xB
#define ACCESS_DENIED_CALLBACK_OBJECT_ACE_TYPE  0xC
#define SYSTEM_AUDIT_CALLBACK_ACE_TYPE          0xD
#define SYSTEM_ALARM_CALLBACK_ACE_TYPE          0xE
#define SYSTEM_AUDIT_CALLBACK_OBJECT_ACE_TYPE   0xF
#define SYSTEM_ALARM_CALLBACK_OBJECT_ACE_TYPE   0x10

/*
 * MS' defined values for ace_flags
 */
#define OBJECT_INHERIT_ACE_FLAG          0x01
#define CONTAINER_INHERIT_ACE_FLAG       0x02
#define NO_PROPAGATE_INHERIT_ACE_FLAG    0x04
#define INHERIT_ONLY_ACE_FLAG            0x08
#define INHERITED_ACE_FLAG               0x10
#define UNDEF_ACE_FLAG                   0x20 /* MS doesn't define it?! */
#define VALID_INHERIT_ACE_FLAGS          0x1F
#define SUCCESSFUL_ACCESS_ACE_FLAG       0x40
#define FAILED_ACCESS_ACE_FLAG           0x80

/*
 * Set PATH/FILE information levels
 */
#define SMB_SFILEINFO_STANDARD                  1
#define SMB_SFILEINFO_EA_SET                    2
#define SMB_SFILEINFO_BASIC_INFO                0x101
#define SMB_SFILEINFO_DISPOSITION_INFO          0x102
#define SMB_SFILEINFO_ALLOCATION_INFO           0x103
#define SMB_SFILEINFO_END_OF_FILE_INFO          0x104
#define SMB_SFILEINFO_UNIX_BASIC                0x200
#define SMB_SFILEINFO_UNIX_LINK                 0x201
#define SMB_SFILEINFO_UNIX_HLINK                0x203
#define SMB_SFILEINFO_POSIX_ACL					0x204
#define SMB_SFILEINFO_POSIX_UNLINK				0x20A
#define SMB_SFILEINFO_UNIX_INFO2				0x20B
#define SMB_SFILEINFO_DIRECTORY_INFORMATION     1001
#define SMB_SFILEINFO_FULL_DIRECTORY_INFORMATION 1002
#define SMB_SFILEINFO_BOTH_DIRECTORY_INFORMATION 1003
#define SMB_SFILEINFO_BASIC_INFORMATION         1004
#define SMB_SFILEINFO_STANDARD_INFORMATION      1005
#define SMB_SFILEINFO_INTERNAL_INFORMATION      1006
#define SMB_SFILEINFO_EA_INFORMATION            1007
#define SMB_SFILEINFO_ACCESS_INFORMATION        1008
#define SMB_SFILEINFO_NAME_INFORMATION          1009
#define SMB_SFILEINFO_RENAME_INFORMATION        1010
#define SMB_SFILEINFO_LINK_INFORMATION          1011
#define SMB_SFILEINFO_NAMES_INFORMATION         1012
#define SMB_SFILEINFO_DISPOSITION_INFORMATION   1013
#define SMB_SFILEINFO_POSITION_INFORMATION      1014
#define SMB_SFILEINFO_1015                      1015 /* ? */
#define SMB_SFILEINFO_MODE_INFORMATION          1016
#define SMB_SFILEINFO_ALIGNMENT_INFORMATION     1017
#define SMB_SFILEINFO_ALL_INFORMATION           1018
#define SMB_SFILEINFO_ALLOCATION_INFORMATION    1019
#define SMB_SFILEINFO_END_OF_FILE_INFORMATION   1020
#define SMB_SFILEINFO_ALT_NAME_INFORMATION      1021
#define SMB_SFILEINFO_STREAM_INFORMATION        1022
#define SMB_SFILEINFO_PIPE_INFORMATION          1023
#define SMB_SFILEINFO_PIPE_LOCAL_INFORMATION    1024
#define SMB_SFILEINFO_PIPE_REMOTE_INFORMATION   1025
#define SMB_SFILEINFO_MAILSLOT_QUERY_INFORMATION 1026
#define SMB_SFILEINFO_MAILSLOT_SET_INFORMATION  1027
#define SMB_SFILEINFO_COMPRESSION_INFORMATION   1028
#define SMB_SFILEINFO_OBJECT_ID_INFORMATION     1029
#define SMB_SFILEINFO_COMPLETION_INFORMATION    1030
#define SMB_SFILEINFO_MOVE_CLUSTER_INFORMATION  1031
#define SMB_SFILEINFO_QUOTA_INFORMATION         1032
#define SMB_SFILEINFO_REPARSE_POINT_INFORMATION 1033
#define SMB_SFILEINFO_NETWORK_OPEN_INFORMATION  1034
#define SMB_SFILEINFO_ATTRIBUTE_TAG_INFORMATION 1035
#define SMB_SFILEINFO_TRACKING_INFORMATION      1036
#define SMB_SFILEINFO_MAXIMUM_INFORMATION	1037

/*
 * LOCKING_ANDX LockType flags
 */
#define SMB_LOCKING_ANDX_SHARED_LOCK	0x01
#define SMB_LOCKING_ANDX_OPLOCK_RELEASE	0x02
#define SMB_LOCKING_ANDX_CHANGE_LOCKTYPE 0x04
#define SMB_LOCKING_ANDX_CANCEL_LOCK	0x08
#define SMB_LOCKING_ANDX_LARGE_FILES	0x10

/* 
 * Definition of parameter block of SMB_SET_POSIX_LOCK
 *
 *   [2 bytes] lock_type - 0 = Read, 1 = Write, 2 = Unlock
 *   [2 bytes] lock_flags - 1 = Wait (only valid for setlock)
 *   [4 bytes] pid = locking context.
 *   [8 bytes] start = unsigned 64 bits.
 *   [8 bytes] length = unsigned 64 bits.
 */

#define POSIX_LOCK_TYPE_OFFSET 0
#define POSIX_LOCK_FLAGS_OFFSET 2
#define POSIX_LOCK_PID_OFFSET 4
#define POSIX_LOCK_START_OFFSET 8
#define POSIX_LOCK_LEN_OFFSET 16
#define POSIX_LOCK_DATA_SIZE 24

#define POSIX_LOCK_FLAG_NOWAIT 0
#define POSIX_LOCK_FLAG_WAIT 1

#define POSIX_LOCK_TYPE_READ 0
#define POSIX_LOCK_TYPE_WRITE 1
#define POSIX_LOCK_TYPE_UNLOCK 2

/* SMB_POSIX_PATH_OPEN "open_mode" definitions. */
#define SMB_O_RDONLY                      0x1
#define SMB_O_WRONLY                      0x2
#define SMB_O_RDWR                        0x4

#define SMB_ACCMODE                       0x7

#define SMB_O_CREAT                      0x10
#define SMB_O_EXCL                       0x20
#define SMB_O_TRUNC                      0x40
#define SMB_O_APPEND                     0x80
#define SMB_O_SYNC                      0x100
#define SMB_O_DIRECTORY                 0x200
#define SMB_O_NOFOLLOW                  0x400
#define SMB_O_DIRECT                    0x800

/*
 * Some names length limitations. Some of them aren't declared by specs,
 * but we need reasonable limits.
 */
#define SMB_MAXNetBIOSNAMELEN	15	/* NetBIOS limit */
#define SMB_MAX_DNS_SRVNAMELEN	255
#define SMB_MAXUSERNAMELEN	128
#define SMB_MAXPASSWORDLEN	128
#define SMB_MAX_NTLM_NAME	(SMB_MAX_DNS_SRVNAMELEN + 1 + SMB_MAXUSERNAMELEN)
/* Max Kerberos principal name length we support */
#define SMB_MAX_KERB_PN		1024
#define SMB_MAX_NATIVE_OS_STRING		256
#define SMB_MAX_NATIVE_LANMAN_STRING	256

/*
 * XP will only allow 80 characters in a share name, the SMB2
 * Spec confirms this in the tree connect section. Since UTF8 
 * can have 3 * 80(characters) bytes then lets make SMB_MAXSHARENAMELEN 
 * 240 bytes.
 */
#define	SMB_MAXSHARENAMELEN		240
#define	SMB_MAXPKTLEN			0x0001FFFF
#define	SMB_LARGE_MAXPKTLEN		0x00FFFFFF	/* Non NetBIOS connections */
#define	SMB_MAXCHALLENGELEN		8
#define	SMB_MAXFNAMELEN			255	/* Keep in sync with MAXNAMLEN */

#define	SMB_RCNDELAY		2	/* seconds between reconnect attempts */
/*
 * leave this zero - we can't ssecond guess server side effects of
 * duplicate ops, this isn't nfs!
 */
#define SMB_MAXSETUPWORDS	3	/* max # of setup words in trans/t2 */

/*
 * Error classes
 */
#define SMBSUCCESS	0x00
#define ERRDOS		0x01
#define ERRSRV		0x02
#define ERRHRD		0x03	/* Error is an hardware error. */
#define ERRCMD		0xFF	/* Command was not in the "SMB" format. */

/*
 * size of the GUID returned in an extended security negotiate response
 */
#define SMB_GUIDLEN	16

typedef uint16_t	smbfh;

#define SMB_NTLM_LEN	21
#define SMB_NTLMV2_LEN	16
#define SMB_LMV2_LEN	24

/*
 * NTLMv2 blob header structure.
 */
struct ntlmv2_blobhdr {
	uint32_t	header;
	uint32_t	reserved;
	uint64_t	timestamp;
	uint64_t	client_nonce;
	uint32_t	unknown1;
};

/*
 * NTLMv2 name header structure, for names in a blob.
 */
struct ntlmv2_namehdr {
	uint16_t	type;
	uint16_t	len;
};

#define NAMETYPE_EOL		0x0000	/* end of list of names */
#define NAMETYPE_MACHINE_NB	0x0001	/* NetBIOS machine name */
#define NAMETYPE_DOMAIN_NB	0x0002	/* NetBIOS domain name */
#define NAMETYPE_MACHINE_DNS	0x0003	/* DNS machine name */
#define NAMETYPE_DOMAIN_DNS	0x0004	/* DNS Active Directory domain name */

/*
 * Named pipe commands.
 */
#define TRANS_CALL_NAMED_PIPE		0x54	/* open/write/read/close pipe */
#define TRANS_WAIT_NAMED_PIPE		0x53	/* wait for pipe to be nonbusy */
#define TRANS_PEEK_NAMED_PIPE		0x23	/* read but don't remove data */
#define TRANS_Q_NAMED_PIPE_HAND_STATE	0x21	/* query pipe handle modes */
#define TRANS_SET_NAMED_PIPE_HAND_STATE	0x01	/* set pipe handle modes */
#define TRANS_Q_NAMED_PIPE_INFO		0x22	/* query pipe attributes */
#define TRANS_TRANSACT_NAMED_PIPE	0x26	/* write/read operation on pipe */
#define TRANS_READ_NAMED_PIPE		0x11	/* read pipe in "raw" (non message mode) */
#define TRANS_WRITE_NAMED_PIPE		0x31	/* write pipe "raw" (non message mode) */  

/*
 * [MS-CIFS]	 
 * WriteMode (2 bytes): A 16-bit field containing flags defined as follows:
 * WritethroughMode 0x0001
 *		If set the server MUST NOT respond to the client before the data is 
 *		written to disk (write-through).
 * ReadBytesAvailable 0x0002
 *		If set the server SHOULD set the Response.SMB_Parameters.Available 
 *		field correctly for writes to named pipes or I/O devices.
 * RAW_MODE 0x0004
 *		Applicable to named pipes only. If set, the named pipe MUST be written 
 *		to in raw mode (no translation).
 * MSG_START 0x0008
 *		Applicable to named pipes only. If set, this data is the start of a message. 
 */
#define WritethroughMode	0x0001
#define ReadBytesAvailable	0x0002
#define RAW_MODE			0x0004
#define MSG_START			0x0008


#define SFM_RESOURCEFORK_NAME	"AFP_Resource"
#define SFM_FINDERINFO_NAME		"AFP_AfpInfo"
#define SFM_DESKTOP_NAME		"AFP_DeskTop"
#define SFM_IDINDEX_NAME		"AFP_IdIndex"

#ifndef XATTR_RESOURCEFORK_NAME
#define XATTR_RESOURCEFORK_NAME		"com.apple.ResourceFork"
#endif
#ifndef XATTR_FINDERINFO_NAME
#define XATTR_FINDERINFO_NAME		"com.apple.FinderInfo"
#endif
#ifndef FINDERINFOSIZE
#define FINDERINFOSIZE 32
#endif
#define SMB_DATASTREAM		":$DATA"

/* 
 * Used in the open/read chain messages 
 *		CreateAndX response is 68 bytes long plus 1 bytes for the word count field
 *		Two more bytes for the createandx byte count field
 *		ReadAndX response is 24 bytes long plus 1 bytes for the word count field
 *		Two more bytes for the readandx byte count field
 *		Eight bytes for pad data between create and read and read and the data buffer
 */
#define SMB_CREATEXRLEN 68 + 1 
#define SMB_READXRLEN 24 +1
#define SMB_BCOUNT_LEN 2
#define SMB_CHAIN_PAD 8
#define SMB_MAX_CHAIN_READ SMB_CREATEXRLEN + SMB_BCOUNT_LEN + SMB_READXRLEN + SMB_BCOUNT_LEN + SMB_CHAIN_PAD
#define SMB_SETUPXRLEN 280	/* This is what Windows 2003 uses not sure why, but better safe than sorry */

#define AFP_INFO_SIZE		60
#define AFP_INFO_FINDER_OFFSET	16

enum stream_types {
	kNoStream = 0,
	kResourceFrk = 1,
	kFinderInfo = 2,
	kExtendedAttr = 4,
	kMsStream = 8
};

#endif /* _NETSMB_SMB_H_ */
