/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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

#ifndef _MSFSCC_H_
#define _MSFSCC_H_


/*
 * Reparse Tags
 *
 * Each reparse point has a reparse tag. The reparse tag uniquely identifies the 
 * owner of that reparse point. The owner is the implementer of the file system   
 * filter driver associated with a reparse tag. Reparse tags are exposed to clients 
 * for third-party applications. Those applications can set, get, and process 
 * reparse tags as needed. Third parties MUST request a reserved reparse tag 
 * value to ensure that conflicting tag values do not occur. The following reparse 
 * tags, with the exception of IO_REPARSE_TAG_SYMLINK, are processed on the server 
 * and are not processed by a client after transmission over the wire. Clients 
 * should treat associated reparse data as opaque data.
 *
 */
#define	IO_REPARSE_TAG_RESERVED_ZERO	0x00000000
#define	IO_REPARSE_TAG_RESERVED_ONE		0x00000001
#define IO_REPARSE_TAG_MOUNT_POINT		0xA0000003
#define IO_REPARSE_TAG_HSM				0xC0000004
#define IO_REPARSE_TAG_HSM2				0x80000006
#define IO_REPARSE_TAG_DRIVER_EXTENDER	0x80000005
#define IO_REPARSE_TAG_SIS				0x80000007
#define IO_REPARSE_TAG_DFS				0x8000000A
#define IO_REPARSE_TAG_DFSR				0x80000012
#define IO_REPARSE_TAG_FILTER_MANAGER	0x8000000B
#define IO_REPARSE_TAG_SYMLINK			0xA000000C


/*
 * A process invokes an FSCTL on a handle to perform an action against the file 
 * or directory associated with the handle. When a server receives an FSCTL request, 
 * it SHOULD use the information in the request, which includes a handle and, 
 * optionally, an input data buffer, to perform the requested action. How a server 
 * performs the action requested by an FSCTL is implementation dependent. The 
 * following specifies the system-defined generic FSCTLs that are permitted to be 
 * invoked across the network. Generic FSCTLs are used by the local file systems 
 * or by multiple components within the system. Any application, service, or 
 * driver may define private FSCTLs. Most private FSCTLs are used locally in the 
 * internal driver stacks and do not flow over the wire. However, if a component 
 * allows its private FSCTLs to flow over the wire, that component is responsible 
 * for ensuring the FSCTLs and associated data structures are documented. 
 * Examples of such private FSCTLs can be found in [MS-SMB2] and [MS-DFSC].
 */
#define FSCTL_CREATE_OR_GET_OBJECT_ID				0x900c0
#define FSCTL_DELETE_OBJECT_ID						0x900a0
#define FSCTL_DELETE_REPARSE_POINT					0x900ac
#define FSCTL_FILESYSTEM_GET_STATISTICS				0x90060
#define FSCTL_FIND_FILES_BY_SID						0x9008f
#define FSCTL_GET_COMPRESSION						0x9003c
#define FSCTL_GET_NTFS_VOLUME_DATA					0x90064
#define FSCTL_GET_OBJECT_ID							0x9009c
#define FSCTL_GET_REPARSE_POINT						0x900a8
#define FSCTL_GET_RETRIEVAL_POINTERS				0x90073
#define FSCTL_IS_PATHNAME_VALID						0x9002c
#define FSCTL_LMR_SET_LINK_TRACKING_INFORMATION		0x1400ec
#define FSCTL_PIPE_PEEK								0x11400c
#define FSCTL_PIPE_TRANSCEIVE						0x11c017
#define FSCTL_PIPE_WAIT								0x110018
#define FSCTL_QUERY_FAT_BPB							0x90058
#define FSCTL_QUERY_ALLOCATED_RANGES				0x940cf
#define FSCTL_QUERY_ON_DISK_VOLUME_INFO				0x9013c
#define FSCTL_QUERY_SPARING_INFO					0x90138
#define FSCTL_READ_FILE_USN_DATA					0x900eb
#define FSCTL_RECALL_FILE							0x90117
#define FSCTL_SET_COMPRESSION						0x9c040
#define FSCTL_SET_DEFECT_MANAGEMENT					0x98134
#define FSCTL_SET_ENCRYPTION						0x900D7
#define FSCTL_SET_OBJECT_ID							0x90098
#define FSCTL_SET_OBJECT_ID_EXTENDED				0x900bc
#define FSCTL_SET_REPARSE_POINT						0x900a4
#define FSCTL_SET_SPARSE							0x900c4
#define FSCTL_SET_ZERO_DATA							0x980c8
#define FSCTL_SET_ZERO_ON_DEALLOCATION				0x90194
#define FSCTL_SIS_COPYFILE							0x90100
#define FSCTL_WRITE_USN_CLOSE_RECORD				0x900ef

/* 
 * Symbolic Link Reparse Data Buffer
 * Flags (4 bytes): A 32-bit field that specifies whether the substitute name is 
 * a full path name or a path name relative to the directory containing the 
 * symbolic link. This field contains one of the following values.
 */
#define SYMLINK_FLAG_ABSOLUTE 0x00000000
#define SYMLINK_FLAG_RELATIVE 0x00000001

#endif // _MSFSCC_H_