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

#include "smbclient.h"
#include "smbclient_private.h"
#include "ntstatus.h"
#include <netsmb/upi_mbuf.h>
#include <netsmb/smbio.h>
#include <netsmb/smb_dev.h>
#include <libkern/OSByteOrder.h>

struct smb_header {
    uint32_t    protocol_id;
    uint8_t     command;
    uint32_t    status;
    uint8_t     flags;
    uint16_t    flags2;
    uint16_t    pid_high;
    uint8_t     signature[8];
    uint16_t    reserved;
    uint16_t    tid;
    uint16_t    pid;
    uint16_t    uid;
    uint16_t    mid;
	/*
     * After the header, we have the following fields:
     *
     * uint8_t  word_count
     *      ...
     *      word_count * sizeof(uint16_t) parameter bytes
     *      ...
     * uint16_t byte_count
     *      ...
     *      byte_count
     *      ...
	 */
} __attribute__((__packed__));

#define ADVANCE(pointer, nbytes) do { \
    (pointer) = (typeof(pointer)) ( ((const uint8_t *)(pointer)) + (nbytes) ); \
} while (0)

static int
smb_ioc_request(
        void *				hContext,
        struct smb_header *	header,
        const mbuf_t		words,
        const mbuf_t		bytes,
        mbuf_t				response)
{
    struct smbioc_rq krq;

	bzero(&krq, sizeof(krq));
    krq.ioc_version = SMB_IOC_STRUCT_VERSION;
    krq.ioc_cmd = header->command;

    /* XXX For large I/O requests where the uint16_t byte count
     * (ioc_tbc) wraps to 0, this interface will get horribly
     * confused. I don't think we can fix this without revving the
     * ioctl version -- jpeach
     */

    /* Set transmit words buffer ... */
    krq.ioc_twc = mbuf_len(words) / sizeof(uint16_t);
    krq.ioc_twords = mbuf_data(words);
    /* Set transmit bytes buffer ... */
    krq.ioc_tbc = mbuf_len(bytes);
    krq.ioc_tbytes = mbuf_data(bytes);
    /* Set receive buffer, reserving space for the word count and byte count ... */
    krq.ioc_rpbufsz = (int32_t)mbuf_maxlen(response);
    krq.ioc_rpbuf = mbuf_data(response);

    if (smb_ioctl_call(((struct smb_ctx *)hContext)->ct_fd,
                SMBIOC_REQUEST, &krq) == -1) {
        return errno;
    }
	
	header->flags = krq.ioc_flags;
	header->flags2 = krq.ioc_flags2;
	header->status = krq.ioc_ntstatus;
	mbuf_setlen(response, krq.ioc_rpbufsz);

    return 0;
}

NTSTATUS
SMBRawTransaction(
    SMBHANDLE   inConnection,
    const void *lpInBuffer,
    size_t		nInBufferSize,
    void		*lpOutBuffer,
    size_t		nOutBufferSize,
    size_t		*lpBytesRead)
{
    NTSTATUS            status;
    int                 err;

    void *              hContext;

    struct smb_header   header;
    mbuf_t				words = NULL;
    mbuf_t				bytes = NULL;
    mbuf_t				response = NULL;
	size_t				len;

    const void * ptr;

	bzero(&header, sizeof(header));
    status = SMBServerContext(inConnection, &hContext);
	if (!NT_SUCCESS(status)) {
        return status;
    }

    if ((size_t)nInBufferSize < sizeof(struct smb_header) ||
        (size_t)nOutBufferSize < sizeof(struct smb_header)) {
 		err = STATUS_BUFFER_TOO_SMALL;
        goto errout;
    }

    /* Split the request buffer into the header, the parameter words and the
     * data bytes.
     */
    ptr = lpInBuffer;

    ADVANCE(ptr, sizeof(uint32_t));
    header.command = *(uint8_t *)ptr;

    if ((size_t)nInBufferSize < (sizeof(struct smb_header) + sizeof(uint8_t))) {
 		err = STATUS_BUFFER_TOO_SMALL;
        goto errout;
    }

    ptr = lpInBuffer;
    ADVANCE(ptr, sizeof(struct smb_header));

	len = (*(uint8_t *)ptr) * sizeof(uint16_t);
	ADVANCE(ptr, sizeof(uint8_t));  /* skip word_count field */
	if (mbuf_attachcluster(MBUF_WAITOK, MBUF_TYPE_DATA, &words, (void *)ptr, NULL, len, NULL)) {
		err = STATUS_NO_MEMORY;
        goto errout;
	}
	mbuf_setlen(words, len);
	ADVANCE(ptr, len);      /* skip parameter words */

    if ((uintptr_t)ptr > ((uintptr_t)lpInBuffer + nInBufferSize) ||
        (uintptr_t)ptr < (uintptr_t)mbuf_data(words)) {
		err = STATUS_MARSHALL_OVERFLOW;
        goto errout;
    }

    len = OSReadLittleInt16(ptr, 0);
    ADVANCE(ptr, sizeof(uint16_t)); /* skip byte_count field */
	if (mbuf_attachcluster(MBUF_WAITOK, MBUF_TYPE_DATA, &bytes, (void *)ptr, NULL, len, NULL)) {
		err = STATUS_NO_MEMORY;
        goto errout;
	}
	mbuf_setlen(bytes, len);
    ADVANCE(ptr, len);

    if ((uintptr_t)ptr > ((uintptr_t)lpInBuffer + nInBufferSize) ||
        (uintptr_t)ptr < (uintptr_t)mbuf_data(bytes)) {
		err = STATUS_MARSHALL_OVERFLOW;
        goto errout;
    }

    /* Set up the response buffer so that we leave room to place the
     * SMB header at the front.
     */
    len = (size_t)(nOutBufferSize - sizeof(struct smb_header));
 	if (mbuf_attachcluster(MBUF_WAITOK, MBUF_TYPE_DATA, &response, ((uint8_t *)lpOutBuffer + sizeof(struct smb_header)), NULL, len, NULL)) {
		err = STATUS_NO_MEMORY;
        goto errout;
    }
	
	err = smb_ioc_request(hContext, &header, words, bytes, response);
    if (err) {
		err = STATUS_IO_DEVICE_ERROR;
        goto errout;
    }

    /* Stash the new SMB header at the front of the output buffer so the
     * caller gets the server status code, etc.
     */
    memcpy(lpOutBuffer, lpInBuffer, sizeof(header));
    OSWriteLittleInt32(lpOutBuffer, 0, *(const uint32_t *)(void *)SMB_SIGNATURE);
    OSWriteLittleInt32(lpOutBuffer, offsetof(struct smb_header, status),
                    header.status);
    OSWriteLittleInt16(lpOutBuffer, offsetof(struct smb_header, flags),
					   header.flags);
    OSWriteLittleInt32(lpOutBuffer, offsetof(struct smb_header, flags2),
                    header.flags2);

    *lpBytesRead = mbuf_len(response) + sizeof(struct smb_header);

    /* We return success, even though the server may have failed the call. The
     * caller is responsible for parsing the reply packet and looking at the
     * status field in the header.
     */
    err = STATUS_SUCCESS;
	
errout:
    if (words)
		mbuf_freem(words);
    if (bytes)
		mbuf_freem(bytes);
    if (response)
		mbuf_freem(response);
	return err;
}

/* vim: set sw=4 ts=4 tw=79 et: */
