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
#include <netsmb/smbio.h>
#include <libkern/OSByteOrder.h>

struct smb_header
{
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
        void *                      hContext,
        struct smb_header *         header,
        const struct smb_lib_mbuf * words,
        const struct smb_lib_mbuf * bytes,
        struct smb_lib_mbuf *       response)
{
    struct smbioc_rq krq = {0};

    krq.ioc_version = SMB_IOC_STRUCT_VERSION;
    krq.ioc_cmd = header->command;

    /* XXX For large I/O requests where the uint16_t byte count
     * (ioc_tbc) wraps to 0, this interface will get horribly
     * confused. I don't think we can fix this without revving the
     * ioctl version -- jpeach
     */

    /* Set transmit words buffer ... */
    krq.ioc_twc = words->m_len / sizeof(uint16_t);
    krq.ioc_twords = SMB_LIB_MTODATA(words, void *);
    /* Set transmit bytes buffer ... */
    krq.ioc_tbc = bytes->m_len;
    krq.ioc_tbytes = SMB_LIB_MTODATA(bytes, void *);
    /* Set receive buffer, reserving space for the word count and byte count ... */
    krq.ioc_rpbufsz = response->m_len - 3;
    krq.ioc_rpbuf = SMB_LIB_MTODATA(response, void *);

    if (smb_ioctl_call(((struct smb_ctx *)hContext)->ct_fd,
                SMBIOC_REQUEST, &krq) == -1) {
        return errno;
    }
	
    header->flags2 = krq.ioc_srflags2;
    header->status = krq.ioc_nt_error;

    /* The response buffer contains the parameter and bytes data packed
     * together (ie. no separated by the byte count and word count). We
     * need to reinsert the word count and byte count to reassemble the
     * packet as it was on the wire. Sigh.
     */

    memmove(SMB_LIB_MTODATA(response, uint8_t *) +
                    (krq.ioc_rwc * sizeof(uint16_t)) + 3,
            SMB_LIB_MTODATA(response, uint8_t *) +
                    (krq.ioc_rwc * sizeof(uint16_t)),
            krq.ioc_rbc);
    OSWriteLittleInt16(response->m_data,
            sizeof(uint8_t) /* word count */ + (krq.ioc_rwc * sizeof(uint16_t)),
            krq.ioc_rbc);

    memmove(SMB_LIB_MTODATA(response, uint8_t *) + 1,
            SMB_LIB_MTODATA(response, uint8_t *),
			(krq.ioc_rwc * sizeof(uint16_t)));
   *SMB_LIB_MTODATA(response, uint8_t *) = krq.ioc_rwc;

    response->m_len = (krq.ioc_rwc * sizeof(uint16_t)) + krq.ioc_rbc + 3;

    /* Response size is the word count + the byte count in units of bytes. */

    return 0;
}

NTSTATUS
SMBRawTransaction(
    SMBHANDLE   hConnection,
    const void *lpInBuffer,
    off_t       nInBufferSize,
    void *      lpOutBuffer,
    off_t       nOutBufferSize,
    off_t *     lpBytesRead)
{
    NTSTATUS            status;
    int                 err;

    void *              hContext;

    struct smb_header   header = {0};
    struct smb_lib_mbuf words = {0};
    struct smb_lib_mbuf bytes = {0};
    struct smb_lib_mbuf response = {0};

    const void * ptr;

    status = SMBServerContext(hConnection, &hContext);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (nInBufferSize < sizeof(struct smb_header) ||
        nOutBufferSize < sizeof(struct smb_header)) {
        return make_nterror(NT_STATUS_BUFFER_TOO_SMALL);
    }

    /* Split the request buffer into the header, the parameter words and the
     * data bytes.
     */
    ptr = lpInBuffer;

    ADVANCE(ptr, sizeof(uint32_t));
    header.command = *(uint8_t *)ptr;

    if (nInBufferSize < (sizeof(struct smb_header) + sizeof(uint8_t))) {
        return make_nterror(NT_STATUS_BUFFER_TOO_SMALL);
    }

    ptr = lpInBuffer;
    ADVANCE(ptr, sizeof(struct smb_header));

    words.m_len = words.m_maxlen = (*(uint8_t *)ptr) * sizeof(uint16_t);
    ADVANCE(ptr, sizeof(uint8_t));  /* skip word_count field */

    words.m_data = (typeof(words.m_data))ptr;
    ADVANCE(ptr, words.m_len);      /* skip parameter words */

    if ((uintptr_t)ptr > ((uintptr_t)lpInBuffer + nInBufferSize) ||
        (uintptr_t)ptr < (uintptr_t)words.m_data) {
        return make_nterror(NT_STATUS_MARSHALL_OVERFLOW);
    }

    bytes.m_len = bytes.m_maxlen = OSReadLittleInt16(ptr, 0);
    ADVANCE(ptr, sizeof(uint16_t)); /* skip byte_count field */

    bytes.m_data = (typeof(bytes.m_data))ptr; 
    ADVANCE(ptr, bytes.m_len);

    if ((uintptr_t)ptr > ((uintptr_t)lpInBuffer + nInBufferSize) ||
        (uintptr_t)ptr < (uintptr_t)bytes.m_data) {
        return make_nterror(NT_STATUS_MARSHALL_OVERFLOW);
    }

    /* Set up the response buffer so that we leave room to place the
     * SMB header at the front.
     */
    response.m_len = response.m_maxlen = nOutBufferSize - sizeof(struct smb_header);
    response.m_data = (typeof(response.m_data))((uint8_t *)lpOutBuffer +
                                                sizeof(struct smb_header));

    err = smb_ioc_request(hContext, &header, &words, &bytes, &response);
    if (err) {
        return make_nterror(NT_STATUS_IO_DEVICE_ERROR);
    }

    /* Stash the new SMB header at the front of the output buffer so the
     * caller gets the server status code, etc.
     */
    memcpy(lpOutBuffer, lpInBuffer, sizeof(header));
    OSWriteLittleInt32(lpOutBuffer, 0, *(const uint32_t *)SMB_SIGNATURE);
    OSWriteLittleInt32(lpOutBuffer, offsetof(struct smb_header, status),
                    header.status);
    OSWriteLittleInt32(lpOutBuffer, offsetof(struct smb_header, flags2),
                    header.flags2);

    *lpBytesRead = response.m_len + sizeof(struct smb_header);

    /* We return success, even though the server may have failed the call. The
     * caller is responsible for parsing the reply packet and looking at the
     * status field in the header.
     */
    return NT_STATUS_SUCCESS;
}

/* vim: set sw=4 ts=4 tw=79 et: */
