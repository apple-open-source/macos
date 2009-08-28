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

#ifndef SMBCLIENT_H_8C21E785_0577_44E0_8CA1_8577A1010DF0
#define SMBCLIENT_H_8C21E785_0577_44E0_8CA1_8577A1010DF0

#include <stdint.h>
#include <sys/types.h>

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
 *
 * In principle, you can use this library by linking only against
 * libsmbclient.a, but for now, we have CoreFoundation dependencies, so you
 * need to -framework CoreFoundation as well.
 *
 * In the future, this will be a part of a SMB private framework.
 */

#if defined(__GNUC__)
#define SMBCLIENT_EXPORT __attribute__((visibility("default")))
#else
#define SMBCLIENT_EXPORT
#endif

#if !defined(_NTSTATUS_DEFINED)
#define _NTSTATUS_DEFINED
typedef uint32_t NTSTATUS;
#endif

#if !defined(NT_SUCCESS)
#define _NT_SUCCESS_DEFINED
static inline bool
NT_SUCCESS(NTSTATUS status) {
        return (status & 0xC0000000) == 0;
}
#endif

#if !defined(NT_STATUS_SUCCESS)
#define NT_STATUS_SUCCESS 0
#endif

struct smb_server_handle;
typedef struct smb_server_handle * SMBHANDLE;

typedef uint64_t SMBFID;

#ifdef SMBCLIENT_PRIVATE
NTSTATUS
SMBServerContext(
    SMBHANDLE hConnection,
    void ** phContext);

/* libsmb defines only the code portion of the NTSTATUS. It silently strips the
 * severity and facility.
 */
static inline NTSTATUS
make_nterror(NTSTATUS s)
{
    return (0xC0000000 /* STATUS_SEVERITY_ERROR */ | s);
}

#endif

typedef enum SMBAuthType
{
    kSMBAuthTypeKerberos,
    kSMBAuthTypeUser,
    kSMBAuthTypeGuest,
    kSMBAuthTypeAnonymous
} SMBAuthType;

/*!
 * @function SMBOpenServer
 * @abstract Connect to a SMB tree
 * @param pTargetServer A UTF-8 encoded UNC name or SMB URL.
 * @param pConnection Filled in with a SMBHANDLE if the connection is
 * successful.
 * This is equivalent to calling SMBOpenServerEx with options of 0L.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBOpenServer(
    const char *pTargetServer,
    SMBHANDLE * pConnection);

/*! Require the connection to be authenticated. The connection strategy is
 * to make a connection that is "as authenticated as possible". This option
 * prevents guest or anonymous connections being made.
 */
#define kSMBOptionRequireAuth   0x00000001

/*! Do not prompt the user for authentication. */
#define kSMBOptionNoPrompt      0x00000002

/*!
 * @function SMBOpenServerEx
 * @abstract Connect to a SMB tree with options
 * @param pTargetServer A UTF-8 encoded UNC name or SMB URL.
 * @param pConnection Filled in with a SMBHANDLE if the connection is
 * successful.
 * @param pOptions SMB
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBOpenServerEx(
    const char *pTargetServer,
    SMBHANDLE * pConnection,
    uint64_t    options);

/*!
 * @function SMBRetainServer
 * @abstract Increments the SMBHANDLE reference count.
 * @param pConnection A SMBHANDLE created by SMBOpenServer.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBRetainServer(
    SMBHANDLE pConnection);

/*!
 * @function SMBReleaseServer
 * @abstract Decrements the SMBHANDLE reference count. If the reference count
 * goes to zero, the server object is destroyed.
 * @param pConnection A SMBHANDLE created by SMBOpenServer.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBReleaseServer(
    SMBHANDLE pConnection);

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

typedef struct SMBServerProperties
{
    SMBAuthType authType;
    SMBDialect  dialect;
    uint64_t    capabilities; /* Either SMB or SMB2 capability flags */
    uint32_t    maxReadBytes;
    uint32_t    maxWriteBytes;
    uint32_t    maxTransactBytes;
    uint32_t    tconFlags;
} SMBServerProperties;

/*!
 * @function SMBServerGetAuthType
 * @abstract Return the authentication type that was used for a connection.
 * @param pConnection A SMBHANDLE created by SMBOpenServer.
 * @param pAuthType A SMBAuthType pointer to be filled in.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBServerGetProperties(
        SMBHANDLE       hConnection,
        SMBServerProperties * pProperties);

#if NOTYET

SMBCLIENT_EXPORT
NTSTATUS
SMBGetSessionKey(
    SMBHANDLE   hConnection,
    uint32_t *  pdwSessionKeyLength,
    uint8_t **  ppSessionKey);

SMBCLIENT_EXPORT
void
SMBFreeSessionKey(
    uint8_t * pSessionKey);

#endif

/*!
 * @function SMBCreateFile
 * @abstract Create of open a file.
 * @param hConnection
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
    SMBHANDLE   hConnection,
    const char * lpFileName,
    uint32_t    dwDesiredAccess,
    uint32_t    dwShareMode,
    void *      lpSecurityAttributes,
    uint32_t    dwCreateDisposition,
    uint32_t    dwFlagsAndAttributes,
    SMBFID *    phFile);

/*!
 * @function SMBRawTransaction
 * @abstract Send and receive a pre-marshalled SMB packet.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBRawTransaction(
    SMBHANDLE   hConnection,
    const void *lpInBuffer,
    off_t       nInBufferSize,
    void *      lpOutBuffer,
    off_t       nOutBufferSize,
    off_t *     lpBytesRead);

/*!
 * @function SMBTransactNamedPipe
 * @abstract Perform a transact operation on an open named pipe.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBTransactNamedPipe(
    SMBHANDLE   hConnection,
    SMBFID      hNamedPipe,
    const void *lpInBuffer,
    off_t       nInBufferSize,
    void *      lpOutBuffer,
    off_t       nOutBufferSize,
    off_t *     lpBytesRead);

/*!
 * @function SMBReadFile
 * @abstract Fread from an open file handle.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBReadFile(
    SMBHANDLE   hConnection,
    SMBFID      hFile,
    void *      lpBuffer,
    off_t       nOffset,
    off_t       nNumberOfBytesToRead,
    off_t *     lpNumberOfBytesRead);

/*!
 * @function SMBWriteFile
 * @abstract Write to an open file handle.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBWriteFile(
    SMBHANDLE   hConnection,
    SMBFID      hFile,
    const void *lpBuffer,
    off_t       nOffset,
    off_t       nNumberOfBytesToWrite,
    off_t *     lpNumberOfBytesWritten);

/*!
 * @function SMBCloseFile
 * @abstract Close an open file handle.
 * @result Returns an NTSTATUS error code.
 */
SMBCLIENT_EXPORT
NTSTATUS
SMBCloseFile(
    SMBHANDLE   hConnection,
    SMBFID      hFile);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* SMBCLIENT_H_8C21E785_0577_44E0_8CA1_8577A1010DF0 */
/* vim: set sw=4 ts=4 tw=79 et: */
