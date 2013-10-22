/*
 * Copyright (c) 2009 - 2012 Apple Inc. All rights reserved.
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

#ifndef SMBCLIENT_H_8C21E785_0577_44E0_8CA1_8577A1010DF0
#define SMBCLIENT_H_8C21E785_0577_44E0_8CA1_8577A1010DF0

#include <Availability.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @header SMB Client API
 * This API provides a relatively high level interface to performing SMB
 * protocol transactions. The API abstracts the version of the SMB protocol
 * as well as the various capabilities supported by the target server.
 */

#if !defined(SMBCLIENT_EXPORT)
#if defined(__GNUC__)
#define SMBCLIENT_EXPORT __attribute__((visibility("default")))
#else
#define SMBCLIENT_EXPORT
#endif
#endif /* SMBCLIENT_EXPORT */
	
#if !defined(_NTSTATUS_DEFINED)
#define _NTSTATUS_DEFINED
typedef uint32_t NTSTATUS;
#endif

#define NT_SUCCESS(status) ((status & 0xC0000000) == STATUS_SUCCESS)

/* NT_STATUS_SUCCESS is being depricated and should not be used */
#if !defined(NT_STATUS_SUCCESS)
#define NT_STATUS_SUCCESS 0
#endif
	
struct smb_server_handle;
typedef struct smb_server_handle * SMBHANDLE;

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

typedef enum SMBAuthType
{
    kSMBAuthTypeAuthenticated,
    kSMBAuthTypeKerberos,
    kSMBAuthTypeUser,
    kSMBAuthTypeGuest,
    kSMBAuthTypeAnonymous
} SMBAuthType;

/*!
 * @function SMBOpenServer
 * @abstract Connect to a SMB tree
 * @param targetServer A UTF-8 encoded UNC name or SMB URL.
 * @param outConnection Filled in with a SMBHANDLE if the connection is
 * successful.
 * This is equivalent to calling SMBOpenServerEx with options of 0L.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBOpenServer(
    const char *targetServer,
    SMBHANDLE * outConnection)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*! Do not prompt the user for authentication. */
#define kSMBOptionNoPrompt				0x00000001
/*! Don't touch the nsmb.conf in the users home directory. */
#define kSMBOptionNoUserPreferences		0x00000002
/*! Force the connection to use a new virtual circuit. */
#define kSMBOptionForceNewSession		0x00000004
/*! Connect as guest. */
#define kSMBOptionAllowGuestAuth		0x00000008
/*! Connect as anonymous. */
#define kSMBOptionAllowAnonymousAuth	0x00000010
/*! Only connect as guest. */
#define kSMBOptionUseGuestOnlyAuth		0x00000020
/*! Only connect as anonymous. */
#define kSMBOptionUseAnonymousOnlyAuth	0x00000040
/*! Create an authenticated session connection, don't tree connect. */
#define kSMBOptionSessionOnly			0x00010000

#define kSMBOptionOnlyAuthMask			(kSMBOptionUseGuestOnlyAuth | \
										kSMBOptionUseAnonymousOnlyAuth)
/*!
 * @function SMBOpenServerEx
 * @abstract Connect to a SMB tree with options
 * @param targetServer A UTF-8 encoded UNC name or SMB URL.
 * @param outConnection Filled in with a SMBHANDLE if the connection is
 * successful.
 * @param pOptions SMB
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBOpenServerEx(
    const char *targetServer,
    SMBHANDLE * outConnection,
    uint64_t    options)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
    ;

/*!
 * @function SMBMountShare
 * @abstract Mount a SMB share
 * @param inConnection A SMBHANDLE created by SMBOpenServer.
 * @param targetShare A UTF-8 encoded share name, may be null.
 * @param mountPoint A UTF-8 encoded mount point that must exist.
 * This is equivalent to calling SMBMountShareEx with , no mount flags, options,
 * file modes or directory modes.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBMountShare(
	SMBHANDLE	inConnection,
	const char	*targetShare,
	const char	*mountPoint)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

	/*! Don't use NTFS Streams even if they are supported by the server. */
#define kSMBMntOptionNoStreams			0x00000001
	/*! Don't use Remote Notifications even if they are supported by the server. */
#define kSMBMntOptionNoNotifcations		0x00000002
	/*! Mount the volume soft, return time out error durring reconnect. */
#define kSMBMntOptionSoftMount			0x00000004
#define kSMBReservedTMMount				0x00000008

/*!
 * @function SMBOpenServerWithMountPoint
 * @abstract Find a shared session from a mounted volume
 * @param pTargetMountPath A UTF-8 encoded mount path.
 * @param pTargetTreeName A UTF-8 encoded tree connect name.
 * @param outConnection Filled in with a SMBHANDLE if the connection is
 * successful.
 * @param pOptions SMB
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBOpenServerWithMountPoint(
	const char	*targetMountPath,
	const char	*targetTreeName,
	SMBHANDLE	*outConnection,
	uint64_t	options)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBRetainServer
 * @abstract Increments the SMBHANDLE reference count.
 * @param inConnection A SMBHANDLE created by SMBOpenServer.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBRetainServer(
    SMBHANDLE inConnection)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBReleaseServer
 * @abstract Decrements the SMBHANDLE reference count. If the reference count
 * goes to zero, the server object is destroyed.
 * @param inConnection A SMBHANDLE created by SMBOpenServer.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBReleaseServer(
    SMBHANDLE inConnection)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * The SMB protocol dialect negotiated for the current connection.
 * Clients can generally assume that kSMBDialectSMB means "NT LM 0.12".
 */
typedef enum SMBDialect
{
    kSMBDialectSMB,
    kSMBDialectSMB2_002,
    kSMBDialectSMB2_1
} SMBDialect;

#define kPropertiesVersion	1

/* includes the c-style null terminator */
#define kMaxSrvNameLen	256

typedef struct SMBServerPropertiesV1
{
	uint32_t	version;
    SMBAuthType authType;
    SMBDialect  dialect;
    uint64_t    capabilities;	/* Either SMB or SMB2 capability flags */
	uint64_t    maxReadBytes;
    uint64_t    maxWriteBytes;
    uint64_t    maxTransactBytes;
	uint32_t    treeOptionalSupport;
	uint64_t    internalFlags;		/* Reserved for internal use */
	char		serverName[kMaxSrvNameLen];
	uint8_t		reserved[1024];
} SMBServerPropertiesV1;

/*!
 * @function SMBGetServerProperties
 * @abstract Return properties about the connection.
 * @param inConnection A SMBHANDLE created by SMBOpenServer.
 * @param outProperties Depending on the version request the properties of the
 * connection
 * @param inVersion The version of the properties requested
 * @param inPropertiesSize The size of the outProperties, must match the size of
 * the version being requested.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBGetServerProperties(
		SMBHANDLE	inConnection,
		void		*outProperties,
		uint32_t	inVersion,
		size_t		inPropertiesSize)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

typedef struct SMBShareAttributes
{
    uint32_t    vc_uid;
    uint32_t    vc_flags;
    uint64_t    vc_misc_flags;
    uint32_t    vc_hflags;
    uint32_t    vc_hflags2;
    uint32_t    vc_smb1_caps;
    uint32_t    vc_smb2_caps;
    uint32_t    ss_flags;
    uint32_t    ss_type;
    uint32_t    ss_caps;
    uint32_t    ss_attrs;
    uint16_t	ss_fstype;
    char		server_name[kMaxSrvNameLen];
} SMBShareAttributes;

/*!
 * @function SMBGetShareAttributes
 * @abstract Return attributes from smb_vc and smb_share.
 * @param inConnection A SMBHANDLE created by SMBOpenServerEx.
 * @param outAttrs is of the type SMBShareAttributes and contains
 * smb_vc and smb_share attributes for a particular share
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBGetShareAttributes(
        SMBHANDLE	inConnection,
        void *outAttrs)
__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_NA)
;

/*!
 * @function SMBCreateFile
 * @abstract Create of open a file.
 * @param inConnection
 * @param lpFileName The UTF-8 encoded file path.
 * @param dwShareMode
 * @param lpSecurityAttributes
 * @param dwCreateDisposition
 * @param dsFlagsAndAttributes
 * @param phFile
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBCreateFile(
    SMBHANDLE   inConnection,
    const char * lpFileName,
    uint32_t    dwDesiredAccess,
    uint32_t    dwShareMode,
    void *      lpSecurityAttributes,
    uint32_t    dwCreateDisposition,
    uint32_t    dwFlagsAndAttributes,
    SMBFID *    phFile)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

	
/*!
 * @function SMBCreateNamedStreamFile
 * @abstract Create of open a file.
 * @param inConnection
 * @param lpFileName The UTF-8 encoded file path.
 * @param lpFileStreamName The UTF-8 encoded file stream name. Must be a legal 
 * named streams that starts with a colon, if null then just the path will be 
 * opened/created.
 * @param dwShareMode
 * @param lpSecurityAttributes
 * @param dwCreateDisposition
 * @param dsFlagsAndAttributes
 * @param phFile
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBCreateNamedStreamFile(
    SMBHANDLE   inConnection,
    const char * lpFileName,
    const char * lpFileStreamName,
    uint32_t    dwDesiredAccess,
    uint32_t    dwShareMode,
    void *      lpSecurityAttributes,
    uint32_t    dwCreateDisposition,
    uint32_t    dwFlagsAndAttributes,
    SMBFID *    phFile)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;
	
/*!
 * @function SMBRawTransaction
 * @abstract Send and receive a pre-marshalled SMB packet.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBRawTransaction(
    SMBHANDLE   inConnection,
    const void *lpInBuffer,
    size_t		nInBufferSize,
    void		*lpOutBuffer,
    size_t		nOutBufferSize,
    size_t		*lpBytesRead)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBTransactMailSlot
 * @abstract Perform a transact operation using a mailslot.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBTransactMailSlot(
	SMBHANDLE   inConnection,
	const char	 *MailSlot,
	const void *sndParamBuffer,
	size_t		sndParamBufferSize,
	void		*rcvParamBuffer,
	size_t		*rcvParamBufferSize,
	void		*rcvDataBuffer,
	size_t		*rcvDataBufferSize)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBTransactNamedPipe
 * @abstract Perform a transact operation on an open named pipe.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBTransactNamedPipe(
    SMBHANDLE   inConnection,
    SMBFID      hNamedPipe,
    const void *inBuffer,
    size_t		inBufferSize,
    void		*outBuffer,
    size_t		outBufferSize,
    size_t		*bytesRead)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBReadFile
 * @abstract Fread from an open file handle.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBReadFile(
    SMBHANDLE   inConnection,
    SMBFID      hFile,
    void *      lpBuffer,
    off_t       nOffset,
    size_t		nNumberOfBytesToRead,
    size_t		*lpNumberOfBytesRead)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBWriteFile
 * @abstract Write to an open file handle.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBWriteFile(
    SMBHANDLE   inConnection,
    SMBFID      hFile,
    const void *lpBuffer,
    off_t       nOffset,
    size_t		nNumberOfBytesToWrite,
    size_t		*lpNumberOfBytesWritten)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBDeviceIoControl
 * @abstract Perform a SMB fsctl on the given file handle.
 *
 * @param inConnection
 * @param hDevice
 * @param dwIoControlCode
 * @param lpInBuffer
 * @param nInBufferSize
 * @param lpOutBuffer
 * @param nOutBufferSize
 * @param lpBytesReturned
 *
 * @return Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBDeviceIoControl(
    SMBHANDLE   inConnection,
    SMBFID      hDevice,
    uint32_t    dwIoControlCode,
    const void *lpInBuffer,
    size_t		nInBufferSize,
    void *      lpOutBuffer,
    size_t		nOutBufferSize,
    size_t *    lpBytesReturned)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBCloseFile
 * @abstract Close an open file handle.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBCloseFile(
    SMBHANDLE   inConnection,
    SMBFID      hFile)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function smbfs_version
 * @abstract return the framework version number.
 * @result framework version number.
 */
SMBCLIENT_EXPORT
int SMBFrameworkVersion(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBCreateURLString
 * @abstract Create a URL string that represents the value passed in. Escape out
 *           any needed character.
 * @param domain	The domain name used for authentication, can be null
 * @param user		The user name used for authentication, can be null
 * @param passwd	The password name used for authentication, can be null
 * @param server	The Server name you wish to connect to, cann't be null
 * @param path		The Share/Path name, can be null
 * @param port		The port you wish to connect on, -1 means use the default.
 * @result			Null terminated string, that can be used by SMBOpenServer.
 */
SMBCLIENT_EXPORT
char *SMBCreateURLString(
        const char *domain,
        const char * user,
        const char * passwd,
        const char *server,
        const char *path,
        int32_t port)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBConvertFromUTF8ToCodePage
 * @abstract Convert from a UTF8 string using the system code page.
 * @param utf8Str The UTF8 string that needs to be converted
 * @param uppercase Uppercase string before returning
 * @result Null terminated string.
 */
SMBCLIENT_EXPORT
char *
SMBConvertFromUTF8ToCodePage(
	const char *utf8Str,
	int uppercase)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBConvertFromCodePageToUTF8
 * @abstract Convert to a UTF8 string using the system code page.
 * @param cpStr The UTF8 string that needs to be converted
 * @result Null terminated UTF8 string.
 */
SMBCLIENT_EXPORT
char *
SMBConvertFromCodePageToUTF8(
	const char *cpStr)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBConvertFromUTF16ToUTF8
 * @abstract Convert to a UTF16 string into a UTF8 string.
 * @param utf8str The UTF8 string that needs to be converted, should be double null
 * terminated.
 * @param maxLen Buffer size of the utf16 string. At most maxLen-1 characters
 * will be converted.
 * @param options Currently not used, future expansion
 * @result Null terminated UTF8 string.
 */
SMBCLIENT_EXPORT
char *
SMBConvertFromUTF16ToUTF8(
	const uint16_t *utf16str,
	size_t maxLen,
	uint64_t options)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBConvertFromUTF8ToUTF16
 * @abstract Convert to a UTF8 string into a UTF16 string.
 * @param utf8str The UTF8 string that needs to be converted, should be null
 * terminated.
 * @param maxLen Buffer size of the utf8 string. At most maxLen-1 characters
 * will be converted.
 * @param options Currently not used, future expansion
 * @result UTF16 string.
 */
SMBCLIENT_EXPORT
uint16_t *
SMBConvertFromUTF8ToUTF16(
	   const char *utf8str,
	   size_t maxLen,
	   uint64_t options)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* SMBCLIENT_H_8C21E785_0577_44E0_8CA1_8577A1010DF0 */
/* vim: set sw=4 ts=4 tw=79 et: */
