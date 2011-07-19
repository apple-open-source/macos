/*
 * Copyright (c) 2008-2010 Apple Inc. All rights reserved.
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
#include "smbclient_private.h"
#include "ntstatus.h"

#include <netsmb/smbio.h>
#include <netsmb/upi_mbuf.h>
#include <netsmb/smb_lib.h>
#include <sys/smb_byte_order.h>
#include <sys/mchain.h>
#include <netsmb/rq.h>
#include <netsmb/smb_converter.h>

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
{
#pragma unused(lpSecurityAttributes)
	struct open_inparms inparms;
 	void *          hContext;
	int				fid;
	int             err;
	NTSTATUS        status;
	
    status = SMBServerContext(inConnection, &hContext);
	if (!NT_SUCCESS(status)) {
        return status;
    }
	memset(&inparms, 0,sizeof(inparms));
	inparms.rights = dwDesiredAccess;
	inparms.attrs = SMB_EFA_NORMAL;
	inparms.shareMode = dwShareMode;
	inparms.disp = dwCreateDisposition;
	inparms.createOptions = dwFlagsAndAttributes;
	/* 
	 * Currently smbio_ntcreatex only returns errno, we need return NTStatusErr 
	 * in the future. For now handle the basic errors here.
	 */ 
	err = smbio_ntcreatex(hContext, lpFileName, NULL, &inparms, NULL, &fid);
	if (err == ENOMEM) {
		return STATUS_NO_MEMORY;
	} else if (err) {
		return STATUS_UNSUCCESSFUL;
	}

   *phFile = fid;
    return STATUS_SUCCESS;
}


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
{
#pragma unused(lpSecurityAttributes)
	struct open_inparms inparms;
 	void *          hContext;
	int             fid;
	int             err;
	NTSTATUS        status;
	
	status = SMBServerContext(inConnection, &hContext);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	memset(&inparms, 0,sizeof(inparms));
	inparms.rights = dwDesiredAccess;
	inparms.attrs = SMB_EFA_NORMAL;
	inparms.shareMode = dwShareMode;
	inparms.disp = dwCreateDisposition;
	inparms.createOptions = dwFlagsAndAttributes;
	/* 
	 * XXX -  map real NTSTATUS code
	 * Currently smbio_ntcreatex only returns errno, we need return NTStatusErr 
	 * in the future. For now handle the basic errors here.
	 */ 
	err = smbio_ntcreatex(hContext, lpFileName, lpFileStreamName, &inparms, 
						  NULL, &fid);
	if (err == ENOMEM) {
		return STATUS_NO_MEMORY;
	} else if (err) {
		return STATUS_UNSUCCESSFUL;
	}
	
	*phFile = fid;
    return STATUS_SUCCESS;
}

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
{
    NTSTATUS status;
    void * hContext;
	int error;
	
    status = SMBServerContext(inConnection, &hContext);
	if (!NT_SUCCESS(status)) {
        return status;
    }
	
    error = smbio_transact(hContext, NULL, 0, MailSlot, 
						 sndParamBuffer, sndParamBufferSize, 
						 NULL, 0,
						 rcvParamBuffer, rcvParamBufferSize, 
						 rcvDataBuffer, rcvDataBufferSize);
    if (error) {
		errno = error;
        /* XXX map real NTSTATUS code */
        return (error == EOVERFLOW) ? STATUS_BUFFER_OVERFLOW : STATUS_UNEXPECTED_IO_ERROR;
    }
	
    return STATUS_SUCCESS;
}

NTSTATUS
SMBTransactNamedPipe(
    SMBHANDLE   inConnection,
    SMBFID		hNamedPipe,
    const void *inBuffer,
    size_t		inBufferSize,
    void *		outBuffer,
    size_t		outBufferSize,
    size_t *	bytesRead)
{
    int error;
    NTSTATUS status;
    void * hContext;
 	const char	*namePipe = "\\PIPE\\"; /* Always just pipe for us */
    uint16_t    setup[2];

    status = SMBServerContext(inConnection, &hContext);
	if (!NT_SUCCESS(status)) {
        return status;
    }	
    setup[0] = TRANS_TRANSACT_NAMED_PIPE;
    setup[1] = (uint16_t)hNamedPipe;
	
    error = smbio_transact(hContext, setup, 2, namePipe, NULL, 0,
						 inBuffer, inBufferSize, NULL, NULL,
						 outBuffer, &outBufferSize);
    if (error) {
		errno = error;
        /* XXX map real NTSTATUS code */
        return (error == EOVERFLOW) ? STATUS_BUFFER_OVERFLOW : STATUS_UNEXPECTED_IO_ERROR;
    }

    *bytesRead = outBufferSize;
    return STATUS_SUCCESS;
}

NTSTATUS
SMBReadFile(
    SMBHANDLE   inConnection,
    SMBFID      hFile,
    void *      lpBuffer,
    off_t       nOffset,
    size_t       nNumberOfBytesToRead,
    size_t		*lpNumberOfBytesRead)
{
    void * hContext;
    NTSTATUS status;
    int err;

    status = SMBServerContext(inConnection, &hContext);
	if (!NT_SUCCESS(status)) {
        return status;
    }

    err = smb_read(hContext, (smbfh)hFile, nOffset,
        (uint32_t)nNumberOfBytesToRead, (char *)lpBuffer);
    if (err == -1) {
        /* XXX map real NTSTATUS code */
        return STATUS_UNSUCCESSFUL;
    }

    *lpNumberOfBytesRead = err;
    return STATUS_SUCCESS;
}

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
{
    struct smb_ctx * ctx;
    struct smbioc_fsctl args;
    NTSTATUS status;

    status = SMBServerContext(inConnection, (void **)&ctx);
	if (!NT_SUCCESS(status)) {
        return status;
    }

    if (hDevice > UINT16_MAX) {
        /* No large file handle support until SMB2. */
        return STATUS_INVALID_HANDLE;
    }

	if (nInBufferSize > INT_MAX || nOutBufferSize > INT_MAX) {
        return STATUS_INVALID_PARAMETER;
	}

	bzero(&args, sizeof(args));
	args.ioc_version = SMB_IOC_STRUCT_VERSION;
	args.ioc_fh = (smbfh)hDevice;
	args.ioc_fsctl = dwIoControlCode;
	args.ioc_tdatacnt = (uint32_t)nInBufferSize;
	args.ioc_rdatacnt = (uint32_t)nOutBufferSize;
	args.ioc_tdata = (void *)lpInBuffer;
	args.ioc_rdata = (void *)lpOutBuffer;

	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_FSCTL, &args) == -1) {
        /* XXX need to map errno */
        return STATUS_INVALID_DEVICE_REQUEST;
	}

	if (NT_SUCCESS(args.ioc_ntstatus)) {
		if (args.ioc_errno) {
			/* XXX need to map errno */
			return STATUS_UNEXPECTED_NETWORK_ERROR;
		}

		if (lpBytesReturned) {
			/* Report updated response data size if the caller wants it. */
			*lpBytesReturned = (size_t)args.ioc_rdatacnt;
		}
	}
    return args.ioc_ntstatus;
}

NTSTATUS
SMBWriteFile(
    SMBHANDLE   inConnection,
    SMBFID      hFile,
    const void *lpBuffer,
    off_t       nOffset,
    size_t		nNumberOfBytesToWrite,
    size_t		*lpNumberOfBytesWritten)
{
    void * hContext;
    NTSTATUS status;
    int err;

    status = SMBServerContext(inConnection, &hContext);
	if (!NT_SUCCESS(status)) {
        return status;
    }

    err = smb_write(hContext, (smbfh)hFile, nOffset,
        (uint32_t)nNumberOfBytesToWrite, (char *)lpBuffer);
    if (err == -1) {
        /* XXX map real NTSTATUS code */
        return STATUS_UNSUCCESSFUL;
    }

    *lpNumberOfBytesWritten = err;
    return STATUS_SUCCESS;
}

NTSTATUS
SMBCloseFile(
    SMBHANDLE   inConnection,
    SMBFID      hFile)
{
    void *          hContext;
    NTSTATUS        status;
    int             err;

    status = SMBServerContext(inConnection, &hContext);
	if (!NT_SUCCESS(status)) {
        return status;
    }
	/* 
	 * XXX -  map real NTSTATUS code
	* Currently smbio_close_file only returns errno, we need return NTStatusErr 
	* in the future. For now handle the basic errors here.
	*/ 
	err = smbio_close_file(hContext, (uint16_t)hFile);
	if (err == ENOMEM) {
		return STATUS_NO_MEMORY;
	} else if (err) {
		return STATUS_UNSUCCESSFUL;
	}
    return STATUS_SUCCESS;
}

/* vim: set sw=4 ts=4 tw=79 et: */
