/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
#ifndef NTSTATUS_H_83E0C5F3_6873_4CAA_ADEF_C95FBDD3625E
#define NTSTATUS_H_83E0C5F3_6873_4CAA_ADEF_C95FBDD3625E

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  NTSTATUS encoding are 32 bit values layed out as follows:
 *
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | | | | | | | | | | |1| | | | | | | | | |2| | | | | | | | | |3| |
 * |0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|0|1|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Sev|C|R| Facility              | Code                          |
 * +-------+-----------------------+-------------------------------+
 *
 *  where
 *
 *      Sev - is the severity code
 *
 *          00 - Success
 *          01 - Informational
 *          10 - Warning
 *          11 - Error
 *
 *      C - is the Customer code flag
 *
 *      R - is a reserved bit
 *
 *      Facility - is the facility code
 *			FACILITY_WINDOWS_CE              24
 *			FACILITY_WINDOWS                 8
 *			FACILITY_URT                     19
 *			FACILITY_UMI                     22
 *			FACILITY_SXS                     23
 *			FACILITY_STORAGE                 3
 *			FACILITY_SSPI                    9
 *			FACILITY_SCARD                   16
 *			FACILITY_SETUPAPI                15
 *			FACILITY_SECURITY                9
 *			FACILITY_RPC                     1
 *			FACILITY_WIN32                   7
 *			FACILITY_CONTROL                 10
 *			FACILITY_NULL                    0
 *			FACILITY_MSMQ                    14
 *			FACILITY_MEDIASERVER             13
 *			FACILITY_INTERNET                12
 *			FACILITY_ITF                     4
 *			FACILITY_HTTP                    25
 *			FACILITY_DPLAY                   21
 *			FACILITY_DISPATCH                2
 *			FACILITY_CONFIGURATION           33
 *			FACILITY_COMPLUS                 17
 *			FACILITY_CERT                    11
 *			FACILITY_BACKGROUNDCOPY          32
 *			FACILITY_ACS                     20
 *			FACILITY_AAF                     18
 *
 *
 *      Code - is the facility's status code 
 *				See ntstatus.inc
 */

#define STATUS_SEVERITY_SUCCESS			0x00000000
#define STATUS_SEVERITY_INFORMATIONAL	0x40000000
#define STATUS_SEVERITY_WARNING			0x80000000
#define STATUS_SEVERITY_ERROR			0xC0000000
#define STATUS_SEVERITY_MASK			0xC0000000

typedef enum nt_status
{
#undef declare_ntstatus
#define declare_ntstatus(name, value) name = value,
#include "ntstatus.inc"
#undef declare_ntstatus
}nt_status;

	
/*
 * [MS-CIFS] - 2.2.2.4 SMB Error Classes and Codes
 * This section provides an overview of status codes that can be returned by the 
 * SMB commands listed in this document, including mappings between the NTSTATUS 
 * codes used in the NT LAN Manager dialect, the SMBSTATUS class/code pairs used 
 * in earlier SMB dialects, and common POSIX equivalents. The POSIX error code 
 * mappings are based upon those used in the Xenix server implementation. This 
 * is not an exhaustive listing and MUST NOT be considered normative.
 * Each command and subcommand description also includes a list of status codes 
 * that are returned by CIFS-compliant servers. Individual implementations can 
 * return status codes from their underlying operating systems; it is up to the 
 * implementer to decide how to interpret those status codes.
 * The listing below is organized by SMBSTATUS Error Class. It shows SMBSTATUS 
 * Error Code values and a general description, as well as mappings from 
 * NTSTATUS values ([MS-ERREF] section 2.3.1) and POSIX-style error codes where 
 * possible. Note that multiple NTSTATUS values can map to a single SMBSTATUS 
 * value.
 *
 * NOTE: Skipping all OS2 errors, then will all default to STATUS_UNSUCCESSFUL
 */
#define SUCCESS_Class	0x00
 
#define ERRDOS_Class	0x01
#define ERRbadfunc		0x0001	/* STATUS_NOT_IMPLEMENTED */
#define ERRbadfile		0x0002	/* STATUS_NO_SUCH_FILE */
#define ERRbadpath		0x0003	/* STATUS_OBJECT_PATH_NOT_FOUND */
#define ERRnofids		0x0004	/* STATUS_TOO_MANY_OPENED_FILES */
#define ERRnoaccess		0x0005	/* STATUS_ACCESS_DENIED */
#define ERRbadfid		0x0006	/* STATUS_INVALID_HANDLE */
#define ERRbadmcb		0x0007	/* STATUS_INSUFF_SERVER_RESOURCES */
#define ERRnomem		0x0008	/* STATUS_NO_MEMORY */
#define ERRbadmem		0x0009	/* STATUS_NO_MEMORY */
#define ERRbadenv		0x000A	/* STATUS_INVALID_PARAMETER */
#define ERRbadformat	0x000B	/* STATUS_INVALID_PARAMETER */
#define ERRbadaccess	0x000C	/* STATUS_ACCESS_DENIED */
#define ERRbaddata		0x000D	/* STATUS_DATA_ERROR */
#define ERRoutofmem		0x000E	/* STATUS_NO_MEMORY */
#define ERRbaddrive		0x000F	/* STATUS_INSUFF_SERVER_RESOURCES */
#define ERRremcd		0x0010	/* STATUS_DIRECTORY_NOT_EMPTY */
#define ERRdiffdevice	0x0011	/* STATUS_NOT_SAME_DEVICE */
#define ERRnofiles		0x0012	/* STATUS_NO_MORE_FILES */
#define ERRwriteprotect	0x0013	/* STATUS_MEDIA_WRITE_PROTECTED */
#define ERRnotready		0x0015	/* STATUS_DEVICE_NOT_READY */
#define ERRbadcmd		0x0016	/* STATUS_SMB_BAD_COMMAND */
#define ERRcrc			0x0017	/* STATUS_DATA_ERROR */
#define ERRbadlength	0x0018	/* STATUS_INFO_LENGTH_MISMATCH */
#define ERRsectornotfound 0x001b /* STATUS_NONEXISTENT_SECTOR */
#define ERRgeneral		0x001F	/* STATUS_UNSUCCESSFUL */
#define ERRbadshare		0x0020	/* STATUS_SHARING_VIOLATION */
#define ERRlock			0x0021	/* STATUS_FILE_LOCK_CONFLICT or STATUS_LOCK_NOT_GRANTED */
#define ERRwrongdisk	0x0022	/* STATUS_WRONG_VOLUME */
#define ERReof			0x0026	/* STATUS_END_OF_FILE */
#define ERRunsup		0x0032	/* STATUS_NOT_SUPPORTED */
#define ERRnoipc		0x0042	/* STATUS_BAD_NETWORK_NAME */
#define ERRnosuchshare	0x0043	/* STATUS_BAD_NETWORK_NAME */
#define ERRtoomanynames	0x0044	/* STATUS_TOO_MANY_NAMES */
#define ERRfilexists	0x0050	/* STATUS_OBJECT_NAME_COLLISION */
#define ERRinvalidparam	0x0057	/* STATUS_INVALID_PARAMETER */
#define ERRinvalidname	0x007b	/* STATUS_OBJECT_NAME_INVALID */
#define ERRunknownlevel	0x007c	/* STATUS_INVALID_LEVEL*/
#define ERRdirnotempty	0x0091	/* STATUS_DIRECTORY_NOT_EMPTY */
#define ERRnotlocked	0x009E	/* STATUS_RANGE_NOT_LOCKED */
#define ERRrename		0x00b7	/* STATUS_OBJECT_NAME_COLLISION */
#define ERRbadpipe		0x00E6	/* STATUS_INVALID_PIPE_STATE */
#define ERRpipebusy		0x00E7	/* STATUS_PIPE_BUSY */
#define ERRpipeclosing	0x00E8	/* STATUS_PIPE_CLOSING */
#define ERRnotconnected 0x00E9	/* STATUS_PIPE_DISCONNECTED */
#define ERRmoredata		0x00EA	/* STATUS_MORE_PROCESSING_REQUIRED */
#define ERRbadealist	0x00FF	/* STATUS_EA_TOO_LARGE */
#define ERReasunsupported 0x011A /* STATUS_EAS_NOT_SUPPORTED */
#define ERRnotifyenumdir 0x03FE	/* STATUS_NOTIFY_ENUM_DIR */
#define ERRinvgroup		0x0997	/* STATUS_NETLOGON_NOT_STARTED */

#define ERRSRV_Class	0x02
#define	ERRerror		0x0001	/* STATUS_INSUFFICIENT_RESOURCES */
#define	ERRbadpw		0x0002	/* STATUS_WRONG_PASSWORD */
#define	ERRbadpath		0x0003	/* STATUS_PATH_NOT_COVERED */
#define	ERRaccess		0x0004	/* STATUS_NETWORK_ACCESS_DENIED */
#define	ERRinvtid		0x0005	/* STATUS_SMB_BAD_TID */
#define	ERRinvnetname	0x0006	/* STATUS_BAD_NETWORK_NAME */
#define	ERRinvdevice	0x0007	/* STATUS_BAD_DEVICE_TYPE */
#define	ERRinvsess		0x0010	/* STATUS_UNSUCCESSFUL */
#define	ERRworking		0x0011	/* STATUS_UNSUCCESSFUL */
#define	ERRnotme		0x0012	/* STATUS_UNSUCCESSFUL */
#define	ERRbadcmd		0x0016	/* STATUS_SMB_BAD_COMMAND */
#define	ERRqfull		0x0031	/* STATUS_PRINT_QUEUE_FULL */
#define	ERRqtoobig		0x0032	/* STATUS_NO_SPOOL_SPACE */
#define	ERRqeof			0x0033	/* STATUS_UNSUCCESSFUL */
#define	ERRinvpfid		0x0034	/* STATUS_PRINT_CANCELLED */
#define	ERRsmbcmd		0x0040	/* STATUS_NOT_IMPLEMENTED */
#define	ERRsrverror		0x0041	/* STATUS_UNEXPECTED_NETWORK_ERROR */
#define	ERRfilespecs	0x0043	/* STATUS_INVALID_HANDLE */
#define	ERRbadpermits	0x0045	/* STATUS_NETWORK_ACCESS_DENIED */
#define	ERRsetattrmode	0x0047	/* STATUS_INVALID_PARAMETER */
#define	ERRtimeout		0x0058	/* STATUS_IO_TIMEOUT */
#define	ERRnoresource	0x0059	/* STATUS_REQUEST_NOT_ACCEPTED */
#define	ERRtoomanyuids	0x005A	/* STATUS_TOO_MANY_SESSIONS */
#define	ERRbaduid		0x005B	/* STATUS_SMB_BAD_UID */
#define	ERRnotconnected 0x00E9	/* STATUS_PIPE_DISCONNECTED */
#define	ERRusempx		0x00FA	/* STATUS_SMB_USE_MPX */
#define	ERRusestd		0x00FB	/* STATUS_SMB_USE_STANDARD */
#define	ERRcontmpx		0x00FC	/* STATUS_SMB_CONTINUE_MPX */
#define	ERRaccountExpired	0x08BF	/* STATUS_ACCOUNT_EXPIRED */
#define	ERRbadClient	0x08C0	/* STATUS_INVALID_WORKSTATION */
#define	ERRbadLogonTime	0x08C1	/* STATUS_INVALID_LOGON_HOURS */
#define	ERRpasswordExpired 0x08C2	/* STATUS_PASSWORD_EXPIRED */
#define	ERRnosupport	0xFFFF	/* STATUS_SMB_NO_SUPPORT */

#define ERRHRD_Class	0x03
#define	ERRnowrite		0x0013	/* STATUS_MEDIA_WRITE_PROTECTED */
#define	ERRbadunit		0x0014	/* STATUS_UNSUCCESSFUL */
#define	ERRnotready		0x0015	/* STATUS_NO_MEDIA_IN_DEVICE */
#define	ERRbadcmd		0x0016	/* STATUS_INVALID_DEVICE_STATE */
#define	ERRdata			0x0017	/* STATUS_DATA_ERROR */
#define	ERRbadreq		0x0018	/* STATUS_DATA_ERROR */
#define	ERRseek			0x0019	/* STATUS_UNSUCCESSFUL */
#define	ERRbadmedia		0x001A	/* STATUS_DISK_CORRUPT_ERROR */
#define	ERRbadsector	0x001B	/* STATUS_NONEXISTENT_SECTOR */
#define	ERRnopaper		0x001C	/* STATUS_DEVICE_PAPER_EMPTY */
#define	ERRwrite		0x001D	/* STATUS_IO_DEVICE_ERROR */
#define	ERRread			0x001E	/* STATUS_IO_DEVICE_ERROR */
#define	ERRgeneral		0x001F	/* STATUS_UNSUCCESSFUL */
#define	ERRbadshare		0x0020	/* STATUS_SHARING_VIOLATION */
#define	ERRlock			0x0021	/* STATUS_FILE_LOCK_CONFLICT */
#define	ERRwrongdisk	0x0022	/* STATUS_WRONG_VOLUME */
#define	ERRFCBunavail	0x0023	/* STATUS_UNSUCCESSFUL */
#define ERRsharebufexc	0x0024	/* STATUS_UNSUCCESSFUL */
#define ERRdiskfull		0x0027	/* STATUS_DISK_FULL */
	
#ifdef __cplusplus
} // extern "C"
#endif

#endif /* NTSTATUS_H_83E0C5F3_6873_4CAA_ADEF_C95FBDD3625E */
/* vim: set sw=4 ts=4 tw=79 et: */
