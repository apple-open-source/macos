/*
 * Copyright (c) 2010-2011 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "config.h"
#include <dce/smb.h>
#include <commonp.h>
#include <com.h>
#include <comprot.h>
#include <comnaf.h>
#include <comp.h>
#include <comsoc_smb.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cnp.h>
#include <npnaf.h>
#include <stddef.h>
#include <sys/param.h>
#include <sys/mount.h>

#if HAVE_SMBCLIENT_FRAMEWORK
#include <SMBClient/smbclient.h>
#include <nttypes.h>
rpc_socket_error_t rpc_smb_ntstatus_to_rpc_error(NTSTATUS status);

#if !defined(kSMBOptionAllowGuestAuth)
#define kSMBOptionAllowGuestAuth 0
#endif

#if !defined(kSMBOptionAllowAnonymousAuth)
#define kSMBOptionAllowAnonymousAuth 0
#endif

#if defined(kPropertiesVersion)
#define HAVE_SMBCLIENT_SMBGETSERVERPROPERTIES 1
#endif

#define SMBCLIENT_CONNECTION_FLAGS ( \
    kSMBOptionNoPrompt          | \
    kSMBOptionAllowGuestAuth    | \
    kSMBOptionAllowAnonymousAuth \
)

#endif /* HAVE_SMBCLIENT_FRAMEWORK */

#if HAVE_LW_BASE_H
#include <lw/base.h>
#endif

#if HAVE_LWIO_LWIO_H
#include <lwio/lwio.h>
#endif

#if HAVE_LWMAPSECURITY_LWMAPSECURITY_H
#include <lwmapsecurity/lwmapsecurity.h>
#endif

#define SMB_SOCKET_LOCK(sock) (rpc__smb_socket_lock(sock))
#define SMB_SOCKET_UNLOCK(sock) (rpc__smb_socket_unlock(sock))

typedef struct rpc_smb_transport_info_s
{
    char* peer_principal;
    struct
    {
        unsigned16 length;
        unsigned char* data;
    } session_key;
#if HAVE_LIKEWISE_LWIO
    PIO_CREDS creds;
#endif
} rpc_smb_transport_info_t, *rpc_smb_transport_info_p_t;

typedef enum rpc_smb_state_e
{
    SMB_STATE_SEND,
    SMB_STATE_RECV,
    SMB_STATE_LISTEN,
    SMB_STATE_ERROR
} rpc_smb_state_t;

typedef struct rpc_smb_buffer_s
{
    size_t capacity;
    unsigned char* base;
    unsigned char* start_cursor;
    unsigned char* end_cursor;
} rpc_smb_buffer_t, *rpc_smb_buffer_p_t;

typedef struct rpc_smb_socket_s
{
    rpc_smb_state_t volatile state;
    rpc_np_addr_t peeraddr;
    rpc_np_addr_t localaddr;
    rpc_smb_transport_info_t info;
#if HAVE_LIKEWISE_LWIO
    PIO_CONTEXT context;
    IO_FILE_HANDLE np;
#elif HAVE_SMBCLIENT_FRAMEWORK
    SMBHANDLE handle;
    SMBFID hFile;
#endif
    size_t maxSendBufferSize;
    size_t maxRecvBufferSize;
    rpc_smb_buffer_t sendbuffer;
    rpc_smb_buffer_t recvbuffer;
    struct
    {
#if HAVE_LIKEWISE_LWIO
        IO_FILE_HANDLE* queue;
#endif
        size_t capacity;
        size_t length;
        int selectfd[2];
    } accept_backlog;
    dcethread* listen_thread;
    dcethread_mutex lock;
    dcethread_cond event;
} rpc_smb_socket_t, *rpc_smb_socket_p_t;

#if HAVE_SMBCLIENT_FRAMEWORK
rpc_socket_error_t
rpc_smb_ntstatus_to_rpc_error(
    NTSTATUS status
    )
{
    switch (status) {

        case STATUS_SUCCESS:
            return RPC_C_SOCKET_OK;

        case STATUS_UNEXPECTED_IO_ERROR:
            return RPC_C_SOCKET_EIO;

        case STATUS_CONNECTION_REFUSED:
            return RPC_C_SOCKET_ECONNREFUSED;

        case STATUS_NO_SUCH_DEVICE:
            return RPC_C_SOCKET_ENETUNREACH;

        case STATUS_BUFFER_OVERFLOW:
            return RPC_C_SOCKET_ENOSPC;

        case STATUS_NO_MEMORY:
            return RPC_C_SOCKET_ENOMEM;

        case STATUS_OBJECT_PATH_SYNTAX_BAD:
        case STATUS_INVALID_HANDLE:
        case STATUS_INVALID_PARAMETER:
        case STATUS_UNSUCCESSFUL:
            return RPC_C_SOCKET_EINVAL;

        case STATUS_LOGON_FAILURE:
            return RPC_C_SOCKET_EAUTH;

        case STATUS_BAD_NETWORK_NAME:
            return RPC_C_SOCKET_ENOENT;

        default:
            RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc_smb_ntstatus_to_rpc_error - unmapped ntstatus 0x%x\n", status));
            return (RPC_C_SOCKET_EIO);
    }
}

#endif

void
rpc_smb_transport_info_from_lwio_creds(
    void* creds ATTRIBUTE_UNUSED,
    rpc_transport_info_handle_t* info,
    unsigned32* st
    )
{
    rpc_smb_transport_info_p_t smb_info = NULL;

    smb_info = calloc(1, sizeof(*smb_info));

    if (!smb_info)
    {
        *st = rpc_s_no_memory;
        goto error;
    }

#if HAVE_LIKEWISE_LWIO
    if (LwIoCopyCreds(creds, &smb_info->creds) != 0)
    {
        *st = rpc_s_no_memory;
        goto error;
    }
#endif

    *info = (rpc_transport_info_handle_t) smb_info;

    *st = rpc_s_ok;

error:

    if (*st != rpc_s_ok && smb_info)
    {
        rpc_smb_transport_info_free((rpc_transport_info_handle_t) smb_info);
    }

    return;
}

INTERNAL
void
rpc__smb_transport_info_destroy(
    rpc_smb_transport_info_p_t smb_info
    )
{
    assert(smb_info != NULL);

#if HAVE_LIKEWISE_LWIO
    if (smb_info->creds)
    {
        LwIoDeleteCreds(smb_info->creds);
    }
#endif

    if (smb_info->session_key.data)
    {
        free(smb_info->session_key.data);
    }

    if (smb_info->peer_principal)
    {
        free(smb_info->peer_principal);
    }
}

void
rpc_smb_transport_info_free(
    rpc_transport_info_handle_t info
    )
{
    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc_smb_transport_info_free called\n"));

    if (info)
    {
        rpc__smb_transport_info_destroy((rpc_smb_transport_info_p_t) info);
        free(info);
    }
}

void
rpc_smb_transport_info_inq_session_key(
    rpc_transport_info_handle_t info,
    unsigned char** sess_key,
    unsigned16* sess_key_len
    )
{
    rpc_smb_transport_info_p_t smb_info = (rpc_smb_transport_info_p_t) info;

    if (sess_key)
    {
        *sess_key = smb_info->session_key.data;
    }

    if (sess_key_len)
    {
        *sess_key_len = (unsigned32) smb_info->session_key.length;
    }
}

void
rpc_smb_transport_info_inq_peer_principal_name(
    rpc_transport_info_handle_t info,
    unsigned char** principal
    )
{
    rpc_smb_transport_info_p_t smb_info = (rpc_smb_transport_info_p_t) info;

    if (principal)
    {
        *principal = (unsigned char*) smb_info->peer_principal;
    }
}

INTERNAL
boolean
rpc__smb_transport_info_equal(
    rpc_transport_info_handle_t info1,
    rpc_transport_info_handle_t info2
    )
{
    rpc_smb_transport_info_p_t smb_info1 = (rpc_smb_transport_info_p_t) info1;
    rpc_smb_transport_info_p_t smb_info2 = (rpc_smb_transport_info_p_t) info2;

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_transport_info_equal called\n"));

#if HAVE_LIKEWISE_LWIO
    return
        (smb_info2 == NULL
         && smb_info1->creds == NULL) ||
        (smb_info2 != NULL &&
         ((smb_info1->creds == NULL && smb_info2->creds == NULL) ||
          (smb_info1->creds != NULL && smb_info2->creds != NULL &&
           LwIoCompareCredss(smb_info1->creds, smb_info2->creds))));
#else
    return (smb_info1 == smb_info2);
#endif
}

INTERNAL
inline
size_t
rpc__smb_buffer_pending(
    rpc_smb_buffer_p_t buffer
    )
{
    return buffer->end_cursor - buffer->start_cursor;
}

INTERNAL
inline
size_t
rpc__smb_buffer_length(
    rpc_smb_buffer_p_t buffer
    )
{
    return buffer->end_cursor - buffer->base;
}

INTERNAL
inline
size_t
rpc__smb_buffer_available(
    rpc_smb_buffer_p_t buffer
    )
{
    return (buffer->base + buffer->capacity) - buffer->end_cursor;
}

INTERNAL
inline
rpc_socket_error_t
rpc__smb_buffer_ensure_available(
    rpc_smb_buffer_p_t buffer,
    size_t space
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    unsigned char* new_base = NULL;

    if (!buffer->base)
    {
        buffer->capacity = 2048;
        buffer->base = malloc(buffer->capacity);

        if (!buffer->base)
        {
            serr = RPC_C_SOCKET_ENOMEM;
            goto error;
        }

        buffer->end_cursor = buffer->start_cursor = buffer->base;
    }

    if (space > rpc__smb_buffer_available(buffer))
    {
        while (space > rpc__smb_buffer_available(buffer))
        {
            buffer->capacity *= 2;
        }

        new_base = realloc(buffer->base, buffer->capacity);

        if (!new_base)
        {
            serr = RPC_C_SOCKET_ENOMEM;
            goto error;
        }

        buffer->start_cursor = new_base + (buffer->start_cursor - buffer->base);
        buffer->end_cursor = new_base + (buffer->end_cursor - buffer->base);

        buffer->base = new_base;
    }

error:

    return serr;
}

INTERNAL
size_t
rpc__smb_fragment_size(
    rpc_cn_common_hdr_p_t packet
    )
{
    uint16_t result;
    int packet_order = ((packet->drep[0] >> 4) & 1);
    int native_order;

#if __LITTLE_ENDIAN__
    native_order = (NDR_LOCAL_INT_REP == ndr_c_int_big_endian) ? 0 : 1;
#else
    native_order = (NDR_LOCAL_INT_REP == ndr_c_int_little_endian) ? 0 : 1;
#endif

    if (packet_order != native_order)
    {
        result = SWAB_16(packet->frag_len);
    }
    else
    {
        result = packet->frag_len;
    }

    return (size_t) result;
}

INTERNAL
inline
size_t
rpc__smb_buffer_packet_size(
    rpc_smb_buffer_p_t buffer
    )
{
    rpc_cn_common_hdr_p_t packet = (rpc_cn_common_hdr_p_t) buffer->start_cursor;

    if (rpc__smb_buffer_pending(buffer) < sizeof(*packet))
    {
        return sizeof(*packet);
    }
    else
    {
        return (rpc__smb_fragment_size(packet));
    }
}

INTERNAL
inline
boolean
rpc__smb_buffer_packet_is_last(
    rpc_smb_buffer_p_t buffer
    )
{
    rpc_cn_common_hdr_p_t packet = (rpc_cn_common_hdr_p_t) buffer->start_cursor;

    return (packet->flags & RPC_C_CN_FLAGS_LAST_FRAG) == RPC_C_CN_FLAGS_LAST_FRAG;
}

INTERNAL
inline
rpc_socket_error_t
rpc__smb_buffer_append(
    rpc_smb_buffer_p_t buffer,
    void* data,
    size_t data_size
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    serr = rpc__smb_buffer_ensure_available(buffer, data_size);
    if (serr)
    {
        goto error;
    }

    memcpy(buffer->end_cursor, data, data_size);

    buffer->end_cursor += data_size;

error:

    return serr;
}

INTERNAL
inline
void
rpc__smb_buffer_settle(
    rpc_smb_buffer_p_t buffer
    )
{
    size_t filled = buffer->end_cursor - buffer->start_cursor;
    memmove(buffer->base, buffer->start_cursor, filled);
    buffer->start_cursor = buffer->base;
    buffer->end_cursor = buffer->base + filled;
}

/* Advance buffer start_cursor to the end of the last packet
   or the last packet that is the final fragment in a series,
   whichever comes first.  If the final fragment is found,
   return true, otherwise false.
*/
INTERNAL
inline
boolean
rpc__smb_buffer_advance_cursor(rpc_smb_buffer_p_t buffer, size_t* amount)
{
    boolean last;
    size_t packet_size;

    while (rpc__smb_buffer_packet_size(buffer) <= rpc__smb_buffer_pending(buffer))
    {
        last = rpc__smb_buffer_packet_is_last(buffer);
        packet_size = rpc__smb_buffer_packet_size(buffer);

        buffer->start_cursor += packet_size;

        if (last)
        {
            if (amount)
            {
                *amount = buffer->start_cursor - buffer->base;
            }

            return true;
        }
    }

    return false;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_create(
    rpc_smb_socket_p_t* out
    )
{
    rpc_smb_socket_p_t sock = NULL;
    int err = 0;

    sock = calloc(1, sizeof(*sock));

    if (!sock)
    {
        err = RPC_C_SOCKET_ENOMEM;
        goto done;
    }

    sock->accept_backlog.selectfd[0] = -1;
    sock->accept_backlog.selectfd[1] = -1;

    /* Set up reasonable default local endpoint */
    sock->localaddr.rpc_protseq_id = rpc_c_protseq_id_ncacn_np;
    sock->localaddr.len = offsetof(rpc_np_addr_t, remote_host) + sizeof(sock->localaddr.remote_host);
    sock->localaddr.sa.sun_family = AF_UNIX;
    sock->localaddr.sa.sun_path[0] = '\0';
    sock->localaddr.remote_host[0] = '\0';

    dcethread_mutex_init_throw(&sock->lock, NULL);
    dcethread_cond_init_throw(&sock->event, NULL);

#if HAVE_SMBCLIENT_FRAMEWORK
    sock->handle = NULL;
    sock->hFile = 0;
    sock->maxSendBufferSize = 0;
    sock->maxRecvBufferSize = 0;
#else
    sock->maxSendBufferSize = 8192;
    sock->maxRecvBufferSize = 8192;
#endif

#if HAVE_LIKEWISE_LWIO
    err = LwNtStatusToErrno(LwIoOpenContextShared(&sock->context));
    if (err)
    {
        goto error;
    }
#endif

    *out = sock;

done:

    return err;

#if HAVE_LIKEWISE_LWIO
error:

    if (sock)
    {
        if (sock->context)
        {
            LwIoCloseContext(sock->context);
        }

        dcethread_mutex_destroy_throw(&sock->lock);
        dcethread_cond_destroy_throw(&sock->event);
    }

    goto done;
#endif
}

INTERNAL
void
rpc__smb_socket_destroy(
    rpc_smb_socket_p_t sock
    )
{
#if HAVE_LIKEWISE_LWIO
    size_t i;
#endif

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_destroy called\n"));

    if (!sock)
    {
        return;
    }

#if HAVE_LIKEWISE_LWIO
    if (sock->accept_backlog.queue)
    {
        for (i = 0; i < sock->accept_backlog.capacity; i++)
        {
            if (sock->accept_backlog.queue[i])
            {
                NtCtxCloseFile(sock->context, sock->accept_backlog.queue[i]);
            }
        }

        close(sock->accept_backlog.selectfd[0]);
        close(sock->accept_backlog.selectfd[1]);

        free(sock->accept_backlog.queue);
    }

    if (sock->np && sock->context)
    {
        NtCtxCloseFile(sock->context, sock->np);
    }

    if (sock->context)
    {
        LwIoCloseContext(sock->context);
    }

#elif HAVE_SMBCLIENT_FRAMEWORK

    if (sock->hFile != 0)
    {
        SMBCloseFile(sock->handle, sock->hFile);
        sock->hFile = 0;
    }

    if (sock->handle)
    {
        SMBReleaseServer(sock->handle);
        sock->handle = NULL;
    }

#endif

    if (sock->sendbuffer.base)
    {
        free(sock->sendbuffer.base);
    }

    if (sock->recvbuffer.base)
    {
        free(sock->recvbuffer.base);
    }

    rpc__smb_transport_info_destroy(&sock->info);

    dcethread_mutex_destroy_throw(&sock->lock);
    dcethread_cond_destroy_throw(&sock->event);

    free(sock);
}

INTERNAL
inline
void
rpc__smb_socket_lock(
    rpc_smb_socket_p_t sock
    )
{
    dcethread_mutex_lock_throw(&sock->lock);
}

INTERNAL
inline
void
rpc__smb_socket_unlock(
    rpc_smb_socket_p_t sock
    )
{
    dcethread_mutex_unlock_throw(&sock->lock);
}

INTERNAL
inline
void
rpc__smb_socket_change_state(
    rpc_smb_socket_p_t sock,
    rpc_smb_state_t state
    )
{
    sock->state = state;
    dcethread_cond_broadcast_throw(&sock->event);
}

INTERNAL
inline
void
rpc__smb_socket_wait(
    rpc_smb_socket_p_t sock
    )
{
    DCETHREAD_TRY
    {
        dcethread_cond_wait_throw(&sock->event, &sock->lock);
    }
    DCETHREAD_CATCH_ALL(e)
    {
        dcethread_mutex_unlock(&sock->lock);
        DCETHREAD_RAISE(*e);
    }
    DCETHREAD_ENDTRY;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_construct(
    rpc_socket_t sock,
    rpc_protseq_id_t pseq_id ATTRIBUTE_UNUSED,
#if HAVE_LIKEWISE_LWIO
    rpc_transport_info_handle_t info
#else
    rpc_transport_info_handle_t info ATTRIBUTE_UNUSED
#endif
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb_sock = NULL;
#if HAVE_LIKEWISE_LWIO
    rpc_smb_transport_info_p_t smb_info = (rpc_smb_transport_info_p_t) info;
#endif

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_construct called\n"));

    serr = rpc__smb_socket_create(&smb_sock);

    if (serr)
    {
        goto error;
    }

#if HAVE_LIKEWISE_LWIO
    if (smb_info)
    {
        if (smb_info->creds)
        {
            serr = NtStatusToErrno(LwIoCopyCreds(smb_info->creds, &smb_sock->info.creds));
            if (serr)
            {
                goto error;
            }
        }
    }
#endif

    sock->data.pointer = (void*) smb_sock;

done:

    return serr;

error:

    if (smb_sock)
    {
        rpc__smb_socket_destroy(smb_sock);
    }

    goto done;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_destruct(
    rpc_socket_t sock
    )
{
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;

    rpc__smb_socket_destroy(smb);
    sock->data.pointer = NULL;

    return RPC_C_SOCKET_OK;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_bind(
    rpc_socket_t sock,
    rpc_addr_p_t addr
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_np_addr_p_t npaddr = (rpc_np_addr_p_t) addr;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;

    smb->localaddr = *npaddr;

    return serr;
}

#if HAVE_SMBCLIENT_FRAMEWORK
INTERNAL
rpc_socket_error_t
rpc__smbclient_connect(
    rpc_smb_socket_p_t  smb,
    const char *        netaddr,
    const char *        pipename
)
{
    rpc_socket_error_t  serr = RPC_C_SOCKET_OK;
    char *              smbpath = NULL;
    NTSTATUS            status;
    boolean             have_mountpath = false;
    unsigned_char_p_t   unescaped_netaddr;

    if (netaddr[0] == '/')
    {
        struct statfs fsBuf;

        /* see if its a mountpath instead of a host addr */
        if (statfs ((char *) netaddr, &fsBuf) != 0)
        {
            RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
               ("rpc__smb_socket_connect - statfs failed errno %d\n", errno));
        }
        else
        {
            /* its a mountpath, so use a different api
             to find the existing smb session to share */
            have_mountpath = 1;
        }
    }

    if (have_mountpath == 0)
    {
        /* its not a mount path, so just pass in the URL string */

#if defined(kPropertiesVersion)
        /* unescape the netaddr */
        rpc__string_netaddr_unescape((unsigned_char_p_t) netaddr, 
                                     &unescaped_netaddr, 
                                     &status);
        
        smbpath = SMBCreateURLString(NULL, NULL, NULL, 
                                     (const char *) unescaped_netaddr, 
                                     NULL, -1);
        /* free the escaped netaddr */
        rpc_string_free (&unescaped_netaddr, &status);
#else
        /* Ick. Max OS 10.6 or earlier ... only have the old API. */
        asprintf(&smbpath, "smb://%s", netaddr);
#endif
        if (smbpath == NULL)
        {
            RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
                   ("rpc__smb_socket_connect - smbpath malloc failed\n"));
            serr = RPC_C_SOCKET_ENOMEM;
            goto error;
        }
    }

    /* Never have a username or password here. Either we use an already
     * existing authenticated session, or log in as guest, or fail. Never
     * prompt for a username or password so always set kSMBOptionNoPrompt,
     * but do allow guest or anonymous logins.
     */

    if (have_mountpath == 0)
    {
        RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
               ("rpc__smb_socket_connect - SMBOpenServerEx <%s>\n",
                smbpath));
        status = SMBOpenServerEx(smbpath, &smb->handle, SMBCLIENT_CONNECTION_FLAGS);
    }
    else
    {
        RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
               ("rpc__smb_socket_connect - SMBOpenServerWithMountPoint <%s>\n",
                netaddr));
#if defined(kPropertiesVersion)
        status = SMBOpenServerWithMountPoint(netaddr, NULL,
                        &smb->handle, SMBCLIENT_CONNECTION_FLAGS);
#else
        status = 0xC00000CC; /* STATUS_BAD_NETWORK_NAME */
#endif
    }

    if (!NT_SUCCESS(status))
    {
        RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
               ("rpc__smb_socket_connect - SMBOpenServerEx failed 0x%x\n",
                status));
        serr = rpc_smb_ntstatus_to_rpc_error (status);
        goto error;
    }

    status = SMBCreateFile(smb->handle, pipename,
           GENERIC_READ | GENERIC_WRITE,        /* dwDesiredAccess */
           FILE_SHARE_READ | FILE_SHARE_WRITE,  /* dwShareMode */
           NULL,                                /* lpSecurityAttributes */
           FILE_OPEN,                           /* dwCreateDisposition */
           0x0000,                              /* dwFlagsAndAttributes */
           &smb->hFile);

    if (!NT_SUCCESS(status))
    {
        RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
                       ("rpc__smb_socket_connect - SMBCreateFile failed 0x%x\n",
                        status));
        serr = rpc_smb_ntstatus_to_rpc_error (status);
        goto error;
    }

error:
    if (smbpath)
        free(smbpath);

    return serr;
}

#endif /* HAVE_SMBCLIENT_FRAMEWORK */

INTERNAL
rpc_socket_error_t
rpc__smb_socket_connect(
    rpc_socket_t sock,
    rpc_addr_p_t addr,
    rpc_cn_assoc_t *assoc ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    char *netaddr = NULL;
    unsigned_char_t *endpoint = NULL;
    char *pipename = NULL;
    unsigned32 dbg_status = 0;
#if HAVE_LIKEWISE_LWIO
    PSTR smbpath = NULL;
    PBYTE sesskey = NULL;
    USHORT sesskeylen = 0;
    IO_FILE_NAME filename = { 0 };
    IO_STATUS_BLOCK io_status = { 0 };
    size_t len;
#endif

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_connect called\n"));

    SMB_SOCKET_LOCK(smb);

    /* Break address into host and endpoint */
    rpc__naf_addr_inq_netaddr (addr,
                               (unsigned_char_t**) &netaddr,
                               &dbg_status);
    rpc__naf_addr_inq_endpoint (addr,
                                (unsigned_char_t**) &endpoint,
                                &dbg_status);
    
    RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
                   ("rpc__smb_socket_connect - netaddr <%s>\n", netaddr));
    RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
                   ("rpc__smb_socket_connect - ep <%s>\n", endpoint));

    if (rpc__np_is_valid_endpoint(endpoint, &dbg_status))
    {
        pipename = (char *)endpoint + sizeof("\\pipe\\") - 1;
    }
    else
    {
        serr = RPC_C_SOCKET_EINVAL;
        goto error;
    }

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
                   ("rpc__smb_socket_connect - pipename <%s>\n", pipename));

#if HAVE_LIKEWISE_LWIO
    len = strlen(netaddr) + strlen(pipename) + strlen("\\rdr\\\\IPC$\\") + 1;
    smbpath = malloc(len);
    if (smbpath == NULL) {
        RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
                       ("rpc__smb_socket_connect - smbpath malloc failed\n"));
        serr = RPC_C_SOCKET_ENOMEM;
        goto error;
    }
    snprintf (smbpath, len, "\\rdr\\%s\\IPC$\\%s",
              (char*) netaddr, (char*) pipename);
#elif HAVE_SMBCLIENT_FRAMEWORK
    serr = rpc__smbclient_connect(smb, netaddr, pipename);
#else
    serr = RPC_C_SOCKET_ENOTSUP;
    goto error;
#endif

#if HAVE_LIKEWISE_LWIO

    serr = NtStatusToErrno(
        LwRtlCStringAllocatePrintf(
            &smbpath,
            "\\rdr\\%s\\IPC$\\%s",
            (char*) netaddr,
            (char*) pipename));
    if (serr)
    {
        goto error;
    }

    serr = NtStatusToErrno(
        LwRtlWC16StringAllocateFromCString(
            &filename.FileName,
            smbpath));
    if (serr)
    {
        goto error;
    }

    serr = NtStatusToErrno(
        NtCtxCreateFile(
            smb->context,                            /* IO context */
            smb->info.creds,                         /* Security token */
            &smb->np,                                /* Created handle */
            NULL,                                    /* Async control block */
            &io_status,                              /* Status block */
            &filename,                               /* Filename */
            NULL,                                    /* Security descriptor */
            NULL,                                    /* Security QOS */
            GENERIC_READ | GENERIC_WRITE,            /* Access mode */
            0,                                       /* Allocation size */
            0,                                       /* File attributes */
            FILE_SHARE_READ | FILE_SHARE_WRITE,      /* Sharing mode */
            FILE_OPEN,                               /* Create disposition */
            FILE_CREATE_TREE_CONNECTION,             /* Create options */
            NULL,                                    /* EA buffer */
            0,                                       /* EA buffer length */
            NULL                                     /* ECP List */
            ));
    if (serr)
    {
        goto error;
    }

    serr = NtStatusToErrno(
        LwIoCtxGetSessionKey(
            smb->context,
            smb->np,
            &sesskeylen,
            &sesskey));
    if (serr)
    {
        goto error;
    }

    smb->info.session_key.length = sesskeylen;
    smb->info.session_key.data = malloc(sesskeylen);
    if (!smb->info.session_key.data)
    {
        serr = RPC_C_SOCKET_ENOMEM;
        goto error;
    }
    memcpy(smb->info.session_key.data, sesskey, sesskeylen);
#endif

    /* Save address for future inquiries on this socket */
    memcpy(&smb->peeraddr, addr, sizeof(smb->peeraddr));

    /* Since we did a connect, we will be sending first */
    smb->state = SMB_STATE_SEND;

done:

#if HAVE_LIKEWISE_LWIO
    if (sesskey)
    {
        RtlMemoryFree(sesskey);
    }

    if (filename.FileName)
    {
        RtlMemoryFree(filename.FileName);
    }

    if (smbpath)
    {
        free(smbpath);
    }
#endif

    SMB_SOCKET_UNLOCK(smb);

    // rpc_string_free handles when *ptr is NULL
    rpc_string_free((unsigned_char_t**) &netaddr, &dbg_status);
    rpc_string_free((unsigned_char_t**) &endpoint, &dbg_status);

    return serr;

error:

    goto done;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_accept(
#if HAVE_LIKEWISE_LWIO
    rpc_socket_t sock,
    rpc_addr_p_t addr,
    rpc_socket_t *newsock
#else
   rpc_socket_t sock ATTRIBUTE_UNUSED,
   rpc_addr_p_t addr ATTRIBUTE_UNUSED,
   rpc_socket_t *newsock ATTRIBUTE_UNUSED
#endif
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_accept called\n"));

#if !defined(HAVE_LIKEWISE_LWIO)
    serr = RPC_C_SOCKET_ENOTSUP;
#else
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    rpc_socket_t npsock = NULL;
    rpc_smb_socket_p_t npsmb = NULL;
    IO_FILE_HANDLE np = NULL;
    size_t i;
    char c = 0;
    BYTE clientaddr[4] = {0, 0, 0, 0};
    USHORT clientaddrlen = sizeof(clientaddr);

    *newsock = NULL;

    SMB_SOCKET_LOCK(smb);

    while (smb->accept_backlog.length == 0)
    {
        if (smb->state == SMB_STATE_ERROR)
        {
            serr = -1;
            goto error;
        }

        rpc__smb_socket_wait(smb);
    }

    for (i = 0; i < smb->accept_backlog.capacity; i++)
    {
        if (smb->accept_backlog.queue[i] != NULL)
        {
            np = smb->accept_backlog.queue[i];
            smb->accept_backlog.queue[i] = NULL;
            smb->accept_backlog.length--;
            if (read(smb->accept_backlog.selectfd[0], &c, sizeof(c)) != sizeof(c))
            {
                serr = errno;
                goto error;
            }
            dcethread_cond_broadcast_throw(&smb->event);
            break;
        }
    }

    serr = rpc__socket_open(sock->pseq_id, NULL, &npsock);
    if (serr)
    {
        goto error;
    }

    npsmb = (rpc_smb_socket_p_t) npsock->data.pointer;

    npsmb->np = np;
    np = NULL;

    npsmb->state = SMB_STATE_RECV;

    memcpy(&npsmb->localaddr, &smb->localaddr, sizeof(npsmb->localaddr));

    /* Use our address as a template for client address */
    memcpy(&npsmb->peeraddr, &smb->localaddr, sizeof(npsmb->peeraddr));

    /* Query for client address */
    serr = NtStatusToErrno(
        LwIoCtxGetPeerAddress(
            npsmb->context,
            npsmb->np,
            clientaddr,
            &clientaddrlen));
    if (serr)
    {
        goto error;
    }

    if (clientaddrlen == sizeof(clientaddr))
    {
        snprintf(npsmb->peeraddr.remote_host, sizeof(npsmb->peeraddr.remote_host) - 1,
                 "%u.%u.%u.%u", clientaddr[0], clientaddr[1], clientaddr[2], clientaddr[3]);
    }

    if (addr)
    {
        memcpy(addr, &npsmb->peeraddr, sizeof(npsmb->peeraddr));
    }

     serr = NtStatusToErrno(
        LwIoCtxGetPeerPrincipalName(
            npsmb->context,
            npsmb->np,
            &npsmb->info.peer_principal));
    if (serr)
    {
        goto error;
    }

    serr = NtStatusToErrno(
        LwIoCtxGetSessionKey(
            npsmb->context,
            npsmb->np,
            &npsmb->info.session_key.length,
            &npsmb->info.session_key.data));
    if (serr)
    {
        goto error;
    }

    *newsock = npsock;

error:

    if (np)
    {
        NtCtxCloseFile(smb->context, np);
    }

    SMB_SOCKET_UNLOCK(smb);
#endif

    return serr;
}

#if HAVE_LIKEWISE_LWIO
INTERNAL
void*
rpc__smb_socket_listen_thread(void* data)
{
    int serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) data;
    IO_STATUS_BLOCK status_block = { 0 };
    char *endpoint = NULL;
    char *pipename = NULL;
    unsigned32 dbg_status = 0;
    PSTR smbpath = NULL;
    IO_FILE_NAME filename = { 0 };
    size_t i;
    char c = 0;
    LONG64 default_timeout = 0;

    SMB_SOCKET_LOCK(smb);

    while (smb->state != SMB_STATE_LISTEN)
    {
        if (smb->state == SMB_STATE_ERROR)
        {
            goto error;
        }

        rpc__smb_socket_wait(smb);
    }

    /* Extract endpoint */
    rpc__naf_addr_inq_endpoint ((rpc_addr_p_t) &smb->localaddr,
                                (unsigned_char_t**) &endpoint,
                                &dbg_status);

    if (rpc__np_is_valid_endpoint(endpoint, &dbg_status))
    {
        pipename = endpoint + sizeof("\\pipe\\") - 1;
    }
    else
    {
        serr = RPC_C_SOCKET_EINVAL;
        goto error;
    }

    serr = NtStatusToErrno(
        LwRtlCStringAllocatePrintf(
            &smbpath,
            "\\npfs\\%s",
            (char*) pipename));
    if (serr)
    {
        goto error;
    }

    serr = NtStatusToErrno(
        LwRtlWC16StringAllocateFromCString(
            &filename.FileName,
            smbpath));
    if (serr)
    {
        goto error;
    }

    while (smb->state == SMB_STATE_LISTEN)
    {
        SMB_SOCKET_UNLOCK(smb);

        serr = NtStatusToErrno(
            LwNtCtxCreateNamedPipeFile(
                smb->context,                            /* IO context */
                NULL,                                    /* Security token */
                &smb->np,                                /* NP handle */
                NULL,                                    /* Async control */
                &status_block,                           /* IO status block */
                &filename,                               /* Filename */
                NULL,                                    /* Security descriptor */
                NULL,                                    /* Security QOS */
                GENERIC_READ | GENERIC_WRITE,            /* Desired access mode */
                FILE_SHARE_READ | FILE_SHARE_WRITE,      /* Share access mode */
                FILE_CREATE,                             /* Create disposition */
                0,                                       /* Create options */
                0,                                       /* Named pipe type */
                0,                                       /* Read mode */
                0,                                       /* Completion mode */
                smb->accept_backlog.capacity,            /* Maximum instances */
                0,                                       /* Inbound quota */
                0,                                       /* Outbound quota */
                &default_timeout                         /* Default timeout */
                ));
        if (serr)
        {
            SMB_SOCKET_LOCK(smb);
            goto error;
        }

        serr = NtStatusToErrno(
            LwIoCtxConnectNamedPipe(
                smb->context,
                smb->np,
                NULL,
                &status_block));
        if (serr)
        {
            SMB_SOCKET_LOCK(smb);
            goto error;
        }

        SMB_SOCKET_LOCK(smb);

        /* Wait for a slot to open in the accept queue */
        while (smb->accept_backlog.length == smb->accept_backlog.capacity)
        {
            if (smb->state == SMB_STATE_ERROR)
            {
                goto error;
            }

            rpc__smb_socket_wait(smb);
        }

        /* Put the handle into the accept queue */
        for (i = 0; i < smb->accept_backlog.capacity; i++)
        {
            if (smb->accept_backlog.queue[i] == NULL)
            {
                smb->accept_backlog.queue[i] = smb->np;
                smb->np = NULL;
                smb->accept_backlog.length++;
                if (write(smb->accept_backlog.selectfd[1], &c, sizeof(c)) != sizeof(c))
                {
                    serr = errno;
                    goto error;
                }
                dcethread_cond_broadcast_throw(&smb->event);
                break;
            }
        }
    }

error:
    if (filename.FileName)
    {
        RtlMemoryFree(filename.FileName);
    }

    if (smbpath)
    {
        RtlMemoryFree(smbpath);
    }

    // rpc_string_free handles when *ptr is NULL
    rpc_string_free((unsigned_char_t**) &endpoint, &dbg_status);

    if (serr)
    {
        rpc__smb_socket_change_state(smb, SMB_STATE_ERROR);
    }

    SMB_SOCKET_UNLOCK(smb);

    return NULL;
}
#endif

INTERNAL
rpc_socket_error_t
rpc__smb_socket_listen(
#if HAVE_LIKEWISE_LWIO
   rpc_socket_t sock,
   int backlog
#else
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    int backlog ATTRIBUTE_UNUSED
#endif
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_listen called\n"));

#if !defined(HAVE_LIKEWISE_LWIO)
    serr = RPC_C_SOCKET_ENOTSUP;
    return serr;
#else
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;

    SMB_SOCKET_LOCK(smb);

    smb->accept_backlog.capacity = backlog;
    smb->accept_backlog.length = 0;
    smb->accept_backlog.queue = calloc(backlog, sizeof(*smb->accept_backlog.queue));

    if (!smb->accept_backlog.queue)
    {
        serr = RPC_C_SOCKET_ENOMEM;
        goto error;
    }

    if (pipe(smb->accept_backlog.selectfd) != 0)
    {
        serr = errno;
        goto error;
    }

    smb->state = SMB_STATE_LISTEN;

    dcethread_create_throw(&smb->listen_thread, NULL, rpc__smb_socket_listen_thread, smb);

error:

    SMB_SOCKET_UNLOCK(smb);
#endif
    return serr;
}

#if HAVE_SMBCLIENT_FRAMEWORK
INTERNAL
rpc_socket_error_t
smb_data_send(
#if SMB_NP_NO_TRANSACTIONS
    rpc_socket_t sock
#else
    rpc_socket_t sock ATTRIBUTE_UNUSED
#endif
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb;
    unsigned char* cursor;
    size_t bytes_written = 0;
#if SMB_NP_NO_TRANSACTIONS
    NTSTATUS status;

    smb = (rpc_smb_socket_p_t) sock->data.pointer;
    cursor = smb->sendbuffer.base;
#endif

    do
    {
#if SMB_NP_NO_TRANSACTIONS
        /* <bms> Write the data out without using transactions */
        status = SMBWriteFile(smb->handle,
                              smb->hFile,
                              cursor,
                              0,
                              smb->sendbuffer.start_cursor - cursor,
                              &bytes_written);

        if (!NT_SUCCESS(status))
        {
            RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_do_send - SMBWriteFile failed 0x%x\n", status));
            serr = rpc_smb_ntstatus_to_rpc_error (status);
        }
#else
        /* <bms> for transactions, do nothing here.  Later in the
         receive, we will do the send and receive in a single
         transaction. */
        serr = RPC_C_SOCKET_OK;
        goto error;
#endif

        if (serr)
        {
            goto error;
        }

        cursor += bytes_written;
    } while (cursor < smb->sendbuffer.start_cursor);

    /* Settle the remaining data (which hopefully should be zero if
     the runtime calls us with complete packets) to the start of
     the send buffer */
    rpc__smb_buffer_settle(&smb->sendbuffer);

error:

    return serr;
}

INTERNAL
rpc_socket_error_t
smb_data_do_recv(
    rpc_socket_t sock,
    size_t* count
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    size_t bytes_requested = 0;
    size_t bytes_read = 0;
    NTSTATUS status = STATUS_SUCCESS;
#if !SMB_NP_NO_TRANSACTIONS
    unsigned char* cursor = smb->sendbuffer.base;
    rpc_cn_common_hdr_p_t packet;
    size_t frag_len;
    size_t bytes_written;
    int sent_data = 0;
#endif

    *count = 0;

    do
    {
        serr = rpc__smb_buffer_ensure_available(&smb->recvbuffer, smb->maxRecvBufferSize);
        if (serr)
        {
            goto error;
        }

        bytes_read = 0;
        bytes_requested = rpc__smb_buffer_available(&smb->recvbuffer);
#if SMB_NP_NO_TRANSACTIONS
        /* <bms> Read the data in without using transactions */
        status = SMBReadFile(smb->handle,
                             smb->hFile,
                             smb->recvbuffer.end_cursor,
                             0,
                             bytes_requested,
                             &bytes_read);

        if (!NT_SUCCESS(status))
        {
            RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
                           ("rpc__smb_socket_do_recv - SMBReadFile failed 0x%x\n",
                            status));
            serr = rpc_smb_ntstatus_to_rpc_error (status);
        }
#else
        if ((smb->sendbuffer.start_cursor - cursor) != 0)
        {
            /* Check to see if this is a last fragment or not */
            packet = (rpc_cn_common_hdr_p_t) cursor;
            while (!(packet->flags & RPC_C_CN_FLAGS_LAST_FRAG))
            {
                /* its not a last fragment, so send with SMBWriteFile */
                frag_len = rpc__smb_fragment_size(packet);

                /* Safety check to make sure we are not past the end
                 of the sendbuffer */
                if ( (cursor + frag_len) > smb->sendbuffer.start_cursor)
                {
                    RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
                                   ("rpc__smb_socket_do_recv - past end of send buffer\n"));
                    serr = RPC_C_SOCKET_EIO;
                    goto error;
                }

                bytes_written = 0;
                sent_data = 1;
                status = SMBWriteFile(smb->handle,
                                      smb->hFile,
                                      cursor,
                                      0,
                                      frag_len,
                                      &bytes_written);

                if (!NT_SUCCESS(status))
                {
                    RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
                                   ("rpc__smb_socket_do_send - SMBWriteFile failed 0x%x\n",
                                    status));
                    serr = rpc_smb_ntstatus_to_rpc_error (status);
                }

                if (serr)
                {
                    goto error;
                }
                cursor += bytes_written;
                packet = (rpc_cn_common_hdr_p_t) cursor;
            }
        }

        if ((smb->sendbuffer.start_cursor - cursor) == 0)
        {
            /* <bms> Must be reading in a fragment, so read the data in
             without using transactions */
            status = SMBReadFile(smb->handle,
                                 smb->hFile,
                                 smb->recvbuffer.end_cursor,
                                 0,
                                 bytes_requested,
                                 &bytes_read);
            if (!NT_SUCCESS(status))
            {
                RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
                               ("rpc__smb_socket_do_recv - SMBReadFile failed 0x%x\n",
                                status));
                serr = rpc_smb_ntstatus_to_rpc_error (status);
            }
        }
        else
        {
            /* <bms> for transactions, do send and rcv in a single transaction. */
            sent_data = 1;
            status = SMBTransactNamedPipe(smb->handle,
                                          smb->hFile,
                                          cursor,
                                          smb->sendbuffer.start_cursor - cursor,
                                          smb->recvbuffer.end_cursor,
                                          bytes_requested,
                                          &bytes_read);

            if (!NT_SUCCESS(status))
            {
                RPC_DBG_PRINTF(rpc_e_dbg_general, 7,
                               ("rpc__smb_socket_do_recv - SMBTransactNamedPipe failed 0x%x\n",
                                status));
                serr = rpc_smb_ntstatus_to_rpc_error (status);
            }
        }

        if (sent_data == 1)
        {
            /* assume all the data got sent */
            rpc__smb_buffer_settle(&smb->sendbuffer);
            sent_data = 0;
            cursor = smb->sendbuffer.base;
        }
#endif

        if (status == STATUS_END_OF_FILE)
        {
            serr = RPC_C_SOCKET_OK;
            bytes_read = 0;
        }
        else
        {
            smb->recvbuffer.end_cursor += bytes_read;
        }

        if (((size_t)*count + bytes_read) > SIZE_T_MAX ||
            ((size_t)*count + bytes_read) < (size_t)*count) {
            serr = RPC_C_SOCKET_ENOSPC;
            goto error;
        }

        *count += (size_t)bytes_read;
    } while (NT_SUCCESS(status) && bytes_read == bytes_requested);

error:

    return serr;
}
#endif

INTERNAL
rpc_socket_error_t
rpc__smb_socket_do_send(
    rpc_socket_t sock
    )
{
#if HAVE_SMBCLIENT_FRAMEWORK
    return (smb_data_send (sock));
#elif HAVE_LIKEWISE_LWIO
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    unsigned char* cursor = smb->sendbuffer.base;
    DWORD bytes_written = 0;
    IO_STATUS_BLOCK io_status = { 0 };

    do
    {
        serr = NtStatusToErrno(
            NtCtxWriteFile(
                smb->context,                          /* IO context */
                smb->np,                               /* File handle */
                NULL,                                  /* Async control block */
                &io_status,                            /* IO status block */
                smb->sendbuffer.base,                  /* Buffer */
                smb->sendbuffer.start_cursor - cursor, /* Length */
                NULL,                                  /* Byte offset */
                NULL                                   /* Key */
                ));

        if (serr)
        {
            goto error;
        }

        bytes_written = io_status.BytesTransferred;
        cursor += bytes_written;
    } while (cursor < smb->sendbuffer.start_cursor);

    /* Settle the remaining data (which hopefully should be zero if
       the runtime calls us with complete packets) to the start of
       the send buffer */
    rpc__smb_buffer_settle(&smb->sendbuffer);

error:

    return serr;
#else
    return RPC_C_SOCKET_ENOTSUP;
#endif
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_do_recv(
    rpc_socket_t sock,
    size_t* count
    )
{
#if HAVE_SMBCLIENT_FRAMEWORK
    return (smb_data_do_recv (sock, count));
#elif HAVE_LIKEWISE_LWIO
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    DWORD bytes_requested = 0;
    DWORD bytes_read = 0;
    IO_STATUS_BLOCK io_status = { 0 };
    NTSTATUS status = STATUS_SUCCESS;

    *count = 0;

    do
    {
        serr = rpc__smb_buffer_ensure_available(&smb->recvbuffer, smb->maxRecvBufferSize);
        if (serr)
        {
            goto error;
        }

        bytes_read = 0;
        bytes_requested = rpc__smb_buffer_available(&smb->recvbuffer);

        status = NtCtxReadFile(
            smb->context,                 /* IO context */
            smb->np,                      /* File handle */
            NULL,                         /* Async control block */
            &io_status,                   /* IO status block */
            smb->recvbuffer.end_cursor,   /* Buffer */
            bytes_requested,              /* Length */
            NULL,                         /* Byte offset */
            NULL                          /* Key */
            );
        if (status == STATUS_END_OF_FILE)
        {
            serr = 0;
            bytes_read = 0;
        }
        else
        {
            serr = NtStatusToErrno(status);
            bytes_read = io_status.BytesTransferred;
            smb->recvbuffer.end_cursor += bytes_read;
        }

        if (((off_t)*count + bytes_read) > SIZE_T_MAX ||
            ((off_t)*count + bytes_read) < (off_t)*count) {
            serr = RPC_C_SOCKET_ENOSPC;
            goto error;
        }

        *count += bytes_read;
    } while (NT_SUCCESS(status) && bytes_read == bytes_requested);

error:

    return serr;
#else
        return RPC_C_SOCKET_ENOTSUP;
#endif

}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_sendmsg(
    rpc_socket_t sock,
    rpc_socket_iovec_p_t iov,
    int iov_len,
    rpc_addr_p_t addr ATTRIBUTE_UNUSED,
    size_t *cc
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    int i;
    size_t pending = 0;

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_sendmsg called\n"));

    SMB_SOCKET_LOCK(smb);

    /* Wait until we are in a state where we can send */
    while (smb->state != SMB_STATE_SEND)
    {
        if (smb->state == SMB_STATE_ERROR)
        {
            serr = -1;
            goto error;
        }
        rpc__smb_socket_wait(smb);
    }

    *cc = 0;

    /* Append all fragments into a single buffer */
    for (i = 0; i < iov_len; i++)
    {
        serr = rpc__smb_buffer_append(&smb->sendbuffer, iov[i].iov_base, iov[i].iov_len);

        if (serr)
        {
            goto error;
        }

        *cc += (int)iov[i].iov_len;
    }

    /* Look for the last fragment and do send if we find it */
    if (rpc__smb_buffer_advance_cursor(&smb->sendbuffer, &pending))
    {
        serr = rpc__smb_socket_do_send(sock);
        if (serr)
        {
            goto error;
        }

        /* Switch into recv mode */
        rpc__smb_socket_change_state(smb, SMB_STATE_RECV);
    }

cleanup:

    SMB_SOCKET_UNLOCK(smb);

    return serr;

error:

    rpc__smb_socket_change_state(smb, SMB_STATE_ERROR);

    goto cleanup;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_recvfrom(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    byte_p_t buf ATTRIBUTE_UNUSED,
    int len ATTRIBUTE_UNUSED,
    rpc_addr_p_t from ATTRIBUTE_UNUSED,
    size_t *cc ATTRIBUTE_UNUSED
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_ENOTSUP;

    fprintf(stderr, "WARNING: unsupported smb socket function %s\n", __FUNCTION__);
    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_recvfrom called\n"));

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_recvmsg(
    rpc_socket_t sock,
    rpc_socket_iovec_p_t iov,
    int iov_len,
    rpc_addr_p_t addr,
    size_t *cc
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    int i;
    size_t pending;
    size_t count;

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_recvmsg called\n"));

    SMB_SOCKET_LOCK(smb);

    while (smb->state != SMB_STATE_RECV)
    {
        if (smb->state == SMB_STATE_ERROR)
        {
            serr = -1;
            goto error;
        }
        rpc__smb_socket_wait(smb);
    }

    *cc = 0;

    if (rpc__smb_buffer_length(&smb->recvbuffer) == 0)
    {
        /* Nothing in buffer, read a complete message */
        do
        {
            serr = rpc__smb_socket_do_recv(sock, &count);
            if (serr)
            {
                goto error;
            }
            if (count == 0)
            {
                break;
            }
        } while (!rpc__smb_buffer_advance_cursor(&smb->recvbuffer, NULL));

        /* Reset cursor back to start to begin disperal into scatter buffer */
        smb->recvbuffer.start_cursor = smb->recvbuffer.base;
    }

    for (i = 0; i < iov_len; i++)
    {
        pending = rpc__smb_buffer_pending(&smb->recvbuffer);
        if (iov[i].iov_len < pending)
        {
            memcpy(iov[i].iov_base, smb->recvbuffer.start_cursor, iov[i].iov_len);

            smb->recvbuffer.start_cursor += iov[i].iov_len;
            *cc += (int)iov[i].iov_len;
        }
        else
        {
            memcpy(iov[i].iov_base, smb->recvbuffer.start_cursor, pending);

            *cc += (int)pending;

            /* Reset buffer because we have emptied it */
            smb->recvbuffer.start_cursor = smb->recvbuffer.end_cursor = smb->recvbuffer.base;
            /* Switch into send mode */
            rpc__smb_socket_change_state(smb, SMB_STATE_SEND);
        }
    }

    if (addr)
    {
        memcpy(addr, &smb->peeraddr, sizeof(smb->peeraddr));
    }

cleanup:

    SMB_SOCKET_UNLOCK(smb);

    return serr;

error:

    rpc__smb_socket_change_state(smb, SMB_STATE_ERROR);

    goto cleanup;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_inq_endpoint(
    rpc_socket_t sock,
    rpc_addr_p_t addr
)
{
    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_inq_endpoint called\n"));

    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;

    if (addr->len == 0)
    {
        addr->len = sizeof(addr->sa);
    }

    addr->rpc_protseq_id = smb->localaddr.rpc_protseq_id;
    memcpy(&addr->sa, &smb->localaddr.sa, addr->len);

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_broadcast(
    rpc_socket_t sock ATTRIBUTE_UNUSED
)
{
    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_set_broadcast called\n"));

    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    return serr;
}

#if HAVE_SMBCLIENT_FRAMEWORK && HAVE_SMBCLIENT_SMBGETSERVERPROPERTIES
INTERNAL
rpc__smbclient_set_bufs(
    rpc_socket_t sock,
    unsigned32 *ntxsize,
    unsigned32 *nrxsize
)
{
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    SMBServerPropertiesV1 server_properties;
    NTSTATUS status;

    if ( !smb->handle || (smb->maxSendBufferSize == 0) )
    {
        return (RPC_C_SOCKET_OK);
    }

    memset(&server_properties, 0, sizeof(server_properties));

    /* try to get the max transaction size that is supported */
    status = SMBGetServerProperties(smb->handle, &server_properties,
            kPropertiesVersion, sizeof(server_properties));
    if (NT_SUCCESS(status))
    {
        smb->maxSendBufferSize = (unsigned32) MIN (server_properties.maxWriteBytes, server_properties.maxTransactBytes);
        smb->maxRecvBufferSize = (unsigned32) MIN (server_properties.maxReadBytes, server_properties.maxTransactBytes);

        /* round down to nearest 1K */
        smb->maxSendBufferSize = (smb->maxSendBufferSize / 1024) * 1024;
        smb->maxRecvBufferSize = (smb->maxRecvBufferSize / 1024) * 1024;

        /* fragments can not be bigger than UInt16 */
        if (smb->maxSendBufferSize > UINT16_MAX)
            smb->maxSendBufferSize = UINT16_MAX;

        if (smb->maxRecvBufferSize > UINT16_MAX)
            smb->maxRecvBufferSize = UINT16_MAX;
    }

    *ntxsize = (unsigned32) smb->maxSendBufferSize;
    *nrxsize = (unsigned32) smb->maxRecvBufferSize;

    return (RPC_C_SOCKET_OK);
}
#endif

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_bufs(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    unsigned32 txsize ATTRIBUTE_UNUSED,
    unsigned32 rxsize ATTRIBUTE_UNUSED,
    unsigned32 *ntxsize ATTRIBUTE_UNUSED,
    unsigned32 *nrxsize ATTRIBUTE_UNUSED
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_ENOTSUP;

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_set_bufs called\n"));

#if HAVE_SMBCLIENT_FRAMEWORK && HAVE_SMBCLIENT_SMBGETSERVERPROPERTIES
    serr = rpc__smbclient_set_bufs(sock, ntxsize, nrxsize);
#endif

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_nbio(
    rpc_socket_t sock ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_ENOTSUP;

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_set_nbio called\n"));
    fprintf(stderr, "WARNING: unsupported smb socket function %s\n", __FUNCTION__);

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_close_on_exec(
    rpc_socket_t sock ATTRIBUTE_UNUSED
    )
{
    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_set_close_on_exec called\n"));
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_getpeername(
    rpc_socket_t sock,
    rpc_addr_p_t addr
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_getpeername called\n"));
    SMB_SOCKET_LOCK(smb);

#if HAVE_LIKEWISE_LWIO
    if (!smb->np)
    {
        serr = RPC_C_SOCKET_EINVAL;
        goto error;
    }
#endif

    memcpy(addr, &smb->peeraddr, sizeof(smb->peeraddr));

error:

    SMB_SOCKET_UNLOCK(smb);

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_get_if_id(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    rpc_network_if_id_t *network_if_id ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_get_if_id called\n"));
    *network_if_id = SOCK_STREAM;

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_keepalive(
    rpc_socket_t sock ATTRIBUTE_UNUSED
    )
{
    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_set_keepalive called\n"));
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_nowriteblock_wait(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    struct timeval *tmo ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_ENOTSUP;

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_nowriteblock_wait called\n"));
    fprintf(stderr, "WARNING: unsupported smb socket function %s\n", __FUNCTION__);

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_rcvtimeo(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    struct timeval *tmo ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_set_rcvtimeo called\n"));

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_getpeereid(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    uid_t *euid ATTRIBUTE_UNUSED,
    gid_t *egid ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_ENOTSUP;

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_getpeereid called\n"));
    fprintf(stderr, "WARNING: unsupported smb socket function %s\n", __FUNCTION__);

    return serr;
}

INTERNAL
int
rpc__smb_socket_get_select_desc(
    rpc_socket_t sock
    )
{
    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_get_select_desc called\n"));
   rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    return smb->accept_backlog.selectfd[0];
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_enum_ifaces(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    rpc_socket_enum_iface_fn_p_t efun ATTRIBUTE_UNUSED,
    rpc_addr_vector_p_t *rpc_addr_vec ATTRIBUTE_UNUSED,
    rpc_addr_vector_p_t *netmask_addr_vec ATTRIBUTE_UNUSED,
    rpc_addr_vector_p_t *broadcast_addr_vec ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_ENOTSUP;

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_enum_ifaces called\n"));
    fprintf(stderr, "WARNING: unsupported smb socket function %s\n", __FUNCTION__);

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_inq_transport_info(
#if HAVE_LIKEWISE_LWIO
    rpc_socket_t sock,
#else
    rpc_socket_t sock ATTRIBUTE_UNUSED,
#endif
    rpc_transport_info_handle_t* info
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
#if HAVE_LIKEWISE_LWIO
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
#endif
    rpc_smb_transport_info_p_t smb_info = NULL;

    RPC_DBG_PRINTF(rpc_e_dbg_general, 7, ("rpc__smb_socket_inq_transport_info called\n"));

#if !defined(HAVE_LIKEWISE_LWIO)
    serr = RPC_C_SOCKET_ENOTSUP;
    goto error;
#else
    smb_info = calloc(1, sizeof(*smb_info));

    if (!smb_info)
    {
        serr = RPC_C_SOCKET_ENOMEM;
        goto error;
    }

    if (smb->info.creds)
    {
        serr = NtStatusToErrno(LwIoCopyCreds(smb->info.creds, &smb_info->creds));
        if (serr)
        {
            goto error;
        }
    }

    if (smb->info.peer_principal)
    {
        smb_info->peer_principal = strdup(smb->info.peer_principal);
        if (!smb_info->peer_principal)
        {
            serr = RPC_C_SOCKET_ENOMEM;
            goto error;
        }
    }

    if (smb->info.session_key.data)
    {
        smb_info->session_key.data = malloc(smb->info.session_key.length);
        if (!smb_info->session_key.data)
        {
            serr = RPC_C_SOCKET_ENOMEM;
            goto error;
        }

        memcpy(smb_info->session_key.data, smb->info.session_key.data, smb->info.session_key.length);
        smb_info->session_key.length = smb->info.session_key.length;
    }

    *info = (rpc_transport_info_handle_t) smb_info;
#endif

error:

    if (serr)
    {
        *info = NULL;

        if (smb_info)
        {
            rpc_smb_transport_info_free((rpc_transport_info_handle_t) smb_info);
        }
    }
    return serr;
}

/* ======================================================================== */
/*
 * R P C _ _ S M B _ S O C K E T _ D U P L I C A T E
 *
 * Wrap the native socket representation in a rpc_socket_t. We duplicate the
 * socket file descriptor because we will eventually end up close(2)ing it.
 *
 * Note that we sneakily replace the socket's vtable, thereby turning it into
 * a BSD socket. This is necessary because the native socket representation
 * that comes down from inetd or launchd is a file descriptor. If we used the
 * real SMB vtable, then we would be hooked into the Likewise SMB redirector
 * codem which is not what we want.
 */

INTERNAL rpc_socket_error_t
rpc__smb_socket_duplicate(
    rpc_socket_t        sock,
    rpc_protseq_id_t    protseq_id,
    const void *        sockrep /* pointer to native representation */
    )
{
    const int *         sockfd = (const int *)sockrep;
    rpc_socket_error_t  serr = RPC_C_SOCKET_OK;

    RPC_DBG_GPRINTF(("(rpc__smb_socket_duplicate) sockfd=%d\n",
                sockfd ? *sockfd : -1));

    if (sockfd == NULL || *sockfd == -1) {
        return RPC_C_SOCKET_ENOTSOCK;
    }

    if (protseq_id != rpc_c_protseq_id_ncacn_np)
    {
        return RPC_C_SOCKET_EINVAL;
    }

    rpc__smb_socket_destruct(sock);

    serr = rpc_g_bsd_socket_vtbl.socket_construct(sock,
            rpc_c_protseq_id_ncalrpc, NULL);
    if (RPC_SOCKET_IS_ERR(serr)) {
        return serr;
    }

    // Flip the rpc_socket_t info ASAP in case some higer level
    // calls a generic rpc__socket routine.
    sock->vtbl = &rpc_g_bsd_socket_vtbl;
    sock->pseq_id = protseq_id;

    serr = rpc_g_bsd_socket_vtbl.socket_duplicate(sock,
            rpc_c_protseq_id_ncacn_np, sockrep);
    if (RPC_SOCKET_IS_ERR(serr)) {
        return serr;
    }

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_transport_inq_access_token(
#if HAVE_LIKEWISE_LWMAPSECURITY
    rpc_transport_info_handle_t info,
    rpc_access_token_p_t* token
#else
   rpc_transport_info_handle_t info ATTRIBUTE_UNUSED,
   rpc_access_token_p_t* token ATTRIBUTE_UNUSED
#endif
    )
{
#if HAVE_LIKEWISE_LWMAPSECURITY
    rpc_smb_transport_info_p_t smb_info = (rpc_smb_transport_info_p_t) info;
    NTSTATUS status = STATUS_SUCCESS;
    PLW_MAP_SECURITY_CONTEXT context = NULL;

    status = LwMapSecurityCreateContext(&context);
    if (status) goto error;

    status = LwMapSecurityCreateAccessTokenFromCStringUsername(
        context,
        token,
        smb_info->peer_principal);
    if (status) goto error;

error:

    LwMapSecurityFreeContext(&context);

    return LwNtStatusToErrno(status);
#else
    return RPC_C_SOCKET_ENOTSUP;
#endif
}

rpc_socket_vtbl_t const rpc_g_smb_socket_vtbl =
{
    .socket_duplicate = rpc__smb_socket_duplicate,
    .socket_construct = rpc__smb_socket_construct,
    .socket_destruct = rpc__smb_socket_destruct,
    .socket_bind = rpc__smb_socket_bind,
    .socket_connect = rpc__smb_socket_connect,
    .socket_accept = rpc__smb_socket_accept,
    .socket_listen = rpc__smb_socket_listen,
    .socket_sendmsg = rpc__smb_socket_sendmsg,
    .socket_recvfrom = rpc__smb_socket_recvfrom,
    .socket_recvmsg = rpc__smb_socket_recvmsg,
    .socket_inq_endpoint = rpc__smb_socket_inq_endpoint,
    .socket_set_broadcast = rpc__smb_socket_set_broadcast,
    .socket_set_bufs = rpc__smb_socket_set_bufs,
    .socket_set_nbio = rpc__smb_socket_set_nbio,
    .socket_set_close_on_exec = rpc__smb_socket_set_close_on_exec,
    .socket_getpeername = rpc__smb_socket_getpeername,
    .socket_get_if_id = rpc__smb_socket_get_if_id,
    .socket_set_keepalive = rpc__smb_socket_set_keepalive,
    .socket_nowriteblock_wait = rpc__smb_socket_nowriteblock_wait,
    .socket_set_rcvtimeo = rpc__smb_socket_set_rcvtimeo,
    .socket_getpeereid = rpc__smb_socket_getpeereid,
    .socket_get_select_desc = rpc__smb_socket_get_select_desc,
    .socket_enum_ifaces = rpc__smb_socket_enum_ifaces,
    .socket_inq_transport_info = rpc__smb_socket_inq_transport_info,
    .transport_info_free = rpc_smb_transport_info_free,
    .transport_info_equal = rpc__smb_transport_info_equal,
    .transport_inq_access_token = rpc__smb_socket_transport_inq_access_token
};
