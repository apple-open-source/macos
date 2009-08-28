/*
 * Copyright (c) 2008-2009 Apple Inc. All rights reserved.
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

#include "smbclient.h"
#include <netsmb/smbio.h>

NTSTATUS
SMBCreateFile(
    SMBHANDLE   hConnection,
    const char * lpFileName,
    uint32_t    dwDesiredAccess,
    uint32_t    dwShareMode,
    void *      lpSecurityAttributes,
    uint32_t    dwCreateDisposition,
    uint32_t    dwFlagsAndAttributes,
    SMBFID *    phFile)
{
    struct smb_rq * rqp;
    struct mbdata * mbp;
    uint8_t         wc;
    size_t          namelen, pathlen, i;
    uint16_t        flags2;

    void *          hContext;
    NTSTATUS        status;
    int             err;

    status = SMBServerContext(hConnection, &hContext);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    flags2 = smb_ctx_flags2((struct smb_ctx *)hContext);

    /*
     * Next, open the pipe.
     * XXX - 42 is the biggest reply we expect.
     */
    err = smb_rq_init(hContext, SMB_COM_NT_CREATE_ANDX, 42, &rqp);
    if (err != 0) {
        return make_nterror(NT_STATUS_NO_MEMORY);
    }

    mbp = smb_rq_getrequest(rqp);
    mb_put_uint8(mbp, 0xff);        /* secondary command */
    mb_put_uint8(mbp, 0);           /* MBZ */
    mb_put_uint16le(mbp, 0);        /* offset to next command (none) */
    mb_put_uint8(mbp, 0);           /* MBZ */

    if (flags2 & SMB_FLAGS2_UNICODE) {
        namelen = strlen(lpFileName) + 1;
        pathlen = 2 + (namelen * 2);
    } else {
        namelen = strlen(lpFileName) + 1;
        pathlen = 1 + namelen;
    }

    mb_put_uint16le(mbp, pathlen);
    mb_put_uint32le(mbp, 0);        /* create flags */
    mb_put_uint32le(mbp, 0);        /* FID - basis for path if not root */
    mb_put_uint32le(mbp, dwDesiredAccess);
    mb_put_uint64le(mbp, 0);        /* "initial allocation size" */
    mb_put_uint32le(mbp, SMB_EFA_NORMAL);
    mb_put_uint32le(mbp, dwShareMode);
    mb_put_uint32le(mbp, dwCreateDisposition);
    mb_put_uint32le(mbp, dwFlagsAndAttributes); /* create_options */
    mb_put_uint32le(mbp, NTCREATEX_IMPERSONATION_IMPERSONATION);
    mb_put_uint8(mbp, 0);   /* security flags (?) */
    smb_rq_wend(rqp);

    if (flags2 & SMB_FLAGS2_UNICODE) {
        /* XXX we really ought to correctly convert from UTF-8 to
         * UTF-16LE here, instead of assuming that we have only ASCII
         * characters in the filename.
         */
        mb_put_uint8(mbp, 0);   /* pad byte - needed only for Unicode */
        mb_put_uint16le(mbp, '\\');
        for (i = 0; i < namelen; i++) {
            mb_put_uint16le(mbp, lpFileName[i]);
        }
    } else {
        mb_put_uint8(mbp, '\\');
        for (i = 0; i < namelen; i++) {
            mb_put_uint8(mbp, lpFileName[i]);
        }
    }

    err = smb_rq_simple(rqp);
    if (err != 0) {
        smb_rq_done(rqp);

        /* XXX map real NTSTATUS code */
        return make_nterror(NT_STATUS_UNSUCCESSFUL);
    }

    mbp = smb_rq_getreply(rqp);

    /*
     * spec says 26 for word count, but 34 words are defined
     * and observed from win2000
     */
    wc = rqp->rq_wcount;
    if (wc != 26 && wc != 34 && wc != 42) {
        smb_rq_done(rqp);
        return make_nterror(NT_STATUS_UNEXPECTED_IO_ERROR);
    }

    uint16_t fid16;

    mb_get_uint8(mbp, NULL);        /* secondary cmd */
    mb_get_uint8(mbp, NULL);        /* mbz */
    mb_get_uint16le(mbp, NULL);     /* andxoffset */
    mb_get_uint8(mbp, NULL);        /* oplock lvl granted */
    mb_get_uint16le(mbp, &fid16);      /* FID */

    /* ... more fields that we don't care about ... */

    smb_rq_done(rqp);

    *phFile = fid16;
    return NT_STATUS_SUCCESS;
}

NTSTATUS
SMBTransactNamedPipe(
    SMBHANDLE   hConnection,
    SMBFID      hNamedPipe,
    const void *lpInBuffer,
    off_t       nInBufferSize,
    void *      lpOutBuffer,
    off_t       nOutBufferSize,
    off_t *     lpBytesRead)
{
    ssize_t ret;
    NTSTATUS status;
    void * hContext;

    status = SMBServerContext(hConnection, &hContext);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ret = smbio_transact(hContext,
            (int)hNamedPipe,
            lpInBuffer, nInBufferSize,
            lpOutBuffer, nOutBufferSize);
    if (ret < 0) {
        /* XXX map real NTSTATUS code */
        return (ret == -EOVERFLOW) ? make_nterror(NT_STATUS_BUFFER_OVERFLOW) 
                                : make_nterror(NT_STATUS_UNEXPECTED_IO_ERROR);
    }

    *lpBytesRead = ret;
    return NT_STATUS_SUCCESS;
}

NTSTATUS
SMBReadFile(
    SMBHANDLE   hConnection,
    SMBFID      hFile,
    void *      lpBuffer,
    off_t       nOffset,
    off_t       nNumberOfBytesToRead,
    off_t *     lpNumberOfBytesRead)
{
    void * hContext;
    NTSTATUS status;
    int err;

    status = SMBServerContext(hConnection, &hContext);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    err = smb_read(hContext, (smbfh)hFile, nOffset,
        (uint32_t)nNumberOfBytesToRead, (char *)lpBuffer);
    if (err == -1) {
        /* XXX map real NTSTATUS code */
        return make_nterror(NT_STATUS_UNSUCCESSFUL);
    }

    *lpNumberOfBytesRead = err;
    return NT_STATUS_SUCCESS;
}

NTSTATUS
SMBWriteFile(
    SMBHANDLE   hConnection,
    SMBFID      hFile,
    const void *lpBuffer,
    off_t       nOffset,
    off_t       nNumberOfBytesToWrite,
    off_t *     lpNumberOfBytesWritten)
{
    void * hContext;
    NTSTATUS status;
    int err;

    status = SMBServerContext(hConnection, &hContext);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    err = smb_write(hContext, (smbfh)hFile, nOffset,
        (uint32_t)nNumberOfBytesToWrite, (char *)lpBuffer);
    if (err == -1) {
        /* XXX map real NTSTATUS code */
        return make_nterror(NT_STATUS_UNSUCCESSFUL);
    }

    *lpNumberOfBytesWritten = err;
    return NT_STATUS_SUCCESS;
}

NTSTATUS
SMBCloseFile(
    SMBHANDLE   hConnection,
    SMBFID      hFile)
{
    struct smb_rq * rqp;
    struct mbdata * mbp;
    void *          hContext;
    NTSTATUS        status;
    int             err;

    status = SMBServerContext(hConnection, &hContext);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /*
     * Next, open the pipe.
     * XXX - 8 is the biggest reply we expect.
     */
    err = smb_rq_init(hContext, SMB_COM_CLOSE, 8, &rqp);
    if (err != 0) {
        return make_nterror(NT_STATUS_NO_MEMORY);
    }

    mbp = smb_rq_getrequest(rqp);
    mb_put_uint16le(mbp, (uint16_t)hFile);  /* FID to close */
    mb_put_uint32le(mbp, 0);                /* LastWriteTime (let server set it) */
    smb_rq_wend(rqp);

    err = smb_rq_simple(rqp);
    if (err != 0) {
        smb_rq_done(rqp);
        /* XXX map real NTSTATUS code */
        return make_nterror(NT_STATUS_UNSUCCESSFUL);
    }

    smb_rq_done(rqp);
    return NT_STATUS_SUCCESS;
}

/* vim: set sw=4 ts=4 tw=79 et: */
