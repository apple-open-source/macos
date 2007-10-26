/*
 * 
 * (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
 *
 * Portions Copyright (C) 2004 - 2007 Apple Inc. All rights reserved.
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */
/*
 */
/*
**
**  NAME:
**
**      comsoc.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Veneer over libsmb not provided by the old sock_ or new
**  rpc_{tower,addr}_ components.
**
**
*/

/*
 * Include this before _POSIX_C_SOURCE is defined, so that we get the
 * u_{char,short,int,long} types defined.
 */
#include <sys/types.h>

#include <commonp.h>
#include <com.h>
#include <comprot.h>
#include <comnaf.h>
#include <comp.h>
#include <comsoc_libsmb.h>
#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>

/* ======================================================================== */

/*
 * XXX - is this negotiated somewhere over SMB, or is it just hardcoded?
 */
#define MAX_TRANSACT_SIZE	1024

struct rpc_socket {
    struct smb_ctx  *ctx;         /* libsmb context */
    boolean         pipe_is_open; /* true if pipe is open */
    u_int16_t       fid;          /* if open, FID for pipe */
    boolean         req_sent;     /* true if request sent but reply not yet received */
    rpc_cond_t      req_sent_cond; /* condition variable for req_sent */
    rpc_mutex_t     req_sent_mtx; /* mutex for req_sent */
    int             trans_reply_byte_count;
    char            *trans_reply_data;
    char            trans_reply_buf[MAX_TRANSACT_SIZE];
    boolean         must_read;    /* true if must read more data from pipe */
    rpc_mutex_t     op_state_mtx; /* mutex for operation state */
    unsigned32      ops_in_progress; /* number of operations in progress */
    boolean         free_later;   /* true if we should free when no ops in progress */
};

/* ======================================================================== */

/*
 * R P C _ _ S O C K E T _ N E W
 *
 * Create a new socket for use with a named pipe over SMB.
 */

PRIVATE rpc_socket_error_t rpc__socket_new
(
    rpc_socket_t        *sockp
)
{
    rpc_socket_t    sock;

    RPC_LOG_SOCKET_OPEN_NTR;

    RPC_MEM_ALLOC(
        sock,
        rpc_socket_t,
        sizeof (struct rpc_socket),
        RPC_C_MEM_SOCK_PRIVATE,
        RPC_C_MEM_WAITOK);
    sock->ctx = NULL;
    sock->pipe_is_open = false;
    sock->req_sent = false;
    RPC_MUTEX_INIT(sock->req_sent_mtx);
    RPC_COND_INIT(sock->req_sent_cond, sock->req_sent_mtx);
    sock->trans_reply_byte_count = 0;
    sock->trans_reply_data = NULL;
    RPC_MUTEX_INIT(sock->op_state_mtx);
    sock->ops_in_progress = 0;
    sock->free_later = false;

    RPC_LOG_SOCKET_OPEN_XIT;
    *sockp = sock;
    return ((sock == NULL) ? errno : RPC_C_SOCKET_OK);
}

INTERNAL void free_socket
(
    rpc_socket_t        sock
)
{
    RPC_MUTEX_UNLOCK(sock->op_state_mtx);
    RPC_MUTEX_DELETE(sock->op_state_mtx);
    RPC_COND_DELETE(sock->req_sent_cond, sock->req_sent_mtx);
    RPC_MUTEX_DELETE(sock->req_sent_mtx);
    RPC_MEM_FREE(sock, RPC_C_MEM_SOCK_PRIVATE);
}

/*
 * R P C _ _ S O C K E T _ C L O S E
 *
 * Close (destroy) a socket.
 */

PRIVATE rpc_socket_error_t rpc__socket_close
(
    rpc_socket_t        sock
)
{
    struct smb_rq       *rqp;
    struct mbdata       *mbp;
    rpc_socket_error_t  serr = 0;

    RPC_LOG_SOCKET_CLOSE_NTR;
    if (sock->pipe_is_open)
    {
        /* Close the named pipe */
        serr = smb_rq_init(sock->ctx, SMB_COM_CLOSE, 0, &rqp);
        if (serr != 0)
            return (serr);
        mbp = smb_rq_getrequest(rqp);
        mb_put_uint16le(mbp, sock->fid);
        mb_put_uint32le(mbp, 0);        /* time stamp */
        smb_rq_wend(rqp);
        serr = smb_rq_simple(rqp);
        if (serr != 0)
        {
            smb_rq_done(rqp);
            return (serr);
        }
        mbp = smb_rq_getreply(rqp);
        smb_rq_done(rqp);
        sock->pipe_is_open = false;
    }
    RPC_MUTEX_LOCK(sock->op_state_mtx);
    if (sock->ops_in_progress)
    {
        /*
         * XXX - what happens if a FID on which operations are in
         * progress is closed?  Does it stay open until the operations
         * complete, or do in-progress operations terminate?
         *
         * We can't free the socket, as there are operations in
         * progress on it.  Mark it for deletion later.
         */
        sock->free_later = true;
        RPC_MUTEX_UNLOCK(sock->op_state_mtx);

        /*
         * There will never be more input on this socket, so signal
         * the condition variable.
         */
        RPC_MUTEX_LOCK(sock->req_sent_mtx);
        RPC_COND_SIGNAL(sock->req_sent_cond, sock->req_sent_mtx);
        RPC_MUTEX_UNLOCK(sock->req_sent_mtx);
    }
    else
    {
        /*
         * There are no operations in progress on this socket,
         * so we can free it now.
         */
        free_socket(sock);
    }
    return (serr);
}

/*
 * R P C _ _ S O C K E T _ C O N N E C T
 *
 * Connect a socket to a specified peer's address.
 * This is used only by Connection oriented Protocol Services.
 */

PRIVATE rpc_socket_error_t rpc__socket_connect
(
    rpc_socket_t        sock,
    rpc_addr_p_t        addr
)
{
    rpc_socket_error_t  serr;
    struct smb_rq       *rqp;
    struct mbdata       *mbp;
    u_int8_t            wc;
    size_t              namelen, pathlen, i;
    u_int16_t           flags2;
    
    /*
     * Now we need to create a connection to the server address
     * contained in the RPC address given.
     * First we stash away a pointer to the libsmb context; we assume
     * it is already connected to the IPC$ share.
     */
    sock->ctx = addr->sctx;

    flags2 = smb_ctx_flags2(sock->ctx);

    /*
     * Next, open the pipe.
     * XXX - 42 is the biggest reply we expect.
     */
    serr = smb_rq_init(sock->ctx, SMB_COM_NT_CREATE_ANDX, 42, &rqp);
    if (serr != 0)
        return (serr);
    mbp = smb_rq_getrequest(rqp);
    mb_put_uint8(mbp, 0xff);        /* secondary command */
    mb_put_uint8(mbp, 0);           /* MBZ */
    mb_put_uint16le(mbp, 0);        /* offset to next command (none) */
    mb_put_uint8(mbp, 0);           /* MBZ */
    if (flags2 & SMB_FLAGS2_UNICODE)
    {
        namelen = 2*(strlen(addr->pipe_path) + 1);
        pathlen = 2 + namelen;
    }
    else
    {
        namelen = strlen(addr->pipe_path) + 1;
        pathlen = 1 + namelen;
    }
    mb_put_uint16le(mbp, pathlen);
    mb_put_uint32le(mbp, 0);        /* create flags */
    mb_put_uint32le(mbp, 0);        /* FID - basis for path if not root */
    mb_put_uint32le(mbp, STD_RIGHT_READ_CONTROL_ACCESS|
                         SA_RIGHT_FILE_WRITE_ATTRIBUTES|
                         SA_RIGHT_FILE_READ_ATTRIBUTES|
                         SA_RIGHT_FILE_WRITE_EA|
                         SA_RIGHT_FILE_READ_EA|
                         SA_RIGHT_FILE_APPEND_DATA|
                         SA_RIGHT_FILE_WRITE_DATA|
                         SA_RIGHT_FILE_READ_DATA);
    mb_put_uint64le(mbp, 0);        /* "initial allocation size" */
    mb_put_uint32le(mbp, SMB_EFA_NORMAL);
    mb_put_uint32le(mbp, NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE);
    mb_put_uint32le(mbp, NTCREATEX_DISP_OPEN);
    mb_put_uint32le(mbp, NTCREATEX_OPTIONS_NON_DIRECTORY_FILE);
                                    /* create_options */
    mb_put_uint32le(mbp, NTCREATEX_IMPERSONATION_IMPERSONATION); /* (?) */
    mb_put_uint8(mbp, 0);   /* security flags (?) */
    smb_rq_wend(rqp);
    if (flags2 & SMB_FLAGS2_UNICODE)
    {
        mb_put_uint8(mbp, 0);   /* pad byte - needed only for Unicode */
        mb_put_uint16le(mbp, '\\');
        for (i = 0; i < namelen; i++)
            mb_put_uint16le(mbp, addr->pipe_path[i]);
    }
    else
    {
        mb_put_uint8(mbp, '\\');
        for (i = 0; i < namelen; i++)
            mb_put_uint8(mbp, addr->pipe_path[i]);
    }
    serr = smb_rq_simple(rqp);
    if (serr != 0)
    {
        smb_rq_done(rqp);
        if (serr == EINVAL)
        {
            /*
             * Windows 98, at least - and probably Windows 95
             * and Windows Me - return ERRSRV/ERRerror when we try to
             * open the pipe.  Map that to RPC_C_SOCKET_ECONNREFUSED
             * so that it's treated as an attempt to connect to
             * a port on which nobody's listening, which is probably
             * the best match.
             */
            serr = RPC_C_SOCKET_ECONNREFUSED;
        }
        return (serr);
    }
    mbp = smb_rq_getreply(rqp);
    /*
     * spec says 26 for word count, but 34 words are defined
     * and observed from win2000
     */
    wc = rqp->rq_wcount;
    if (wc != 26 && wc != 34 && wc != 42)
    {
        smb_rq_done(rqp);
        RPC_DBG_PRINTF (rpc_e_dbg_general, RPC_C_CN_DBG_ERRORS,
                        ("(rpc__cn_network_req_connect) ctx->%p pipe->%s open failed, bad word count %u\n",
                         sock->ctx,
                         addr->pa.pipe_path,
                         wc));
        return (RPC_C_SOCKET_EIO);
    }
    mb_get_uint8(mbp, NULL);        /* secondary cmd */
    mb_get_uint8(mbp, NULL);        /* mbz */
    mb_get_uint16le(mbp, NULL);     /* andxoffset */
    mb_get_uint8(mbp, NULL);        /* oplock lvl granted */
    mb_get_uint16le(mbp, &sock->fid); /* FID */
    mb_get_uint32le(mbp, NULL);     /* create_action */
    mb_get_uint64le(mbp, NULL);     /* creation time */
    mb_get_uint64le(mbp, NULL);     /* access time */
    mb_get_uint64le(mbp, NULL);     /* write time */
    mb_get_uint64le(mbp, NULL);     /* change time */
    mb_get_uint32le(mbp, NULL);     /* attributes */
    mb_get_uint64le(mbp, NULL);     /* allocation size */
    mb_get_uint64le(mbp, NULL);     /* EOF */
    mb_get_uint16le(mbp, NULL);     /* file type */
    mb_get_uint16le(mbp, NULL);     /* device state */
    mb_get_uint8(mbp, NULL);        /* directory (boolean) */
    smb_rq_done(rqp);
    sock->pipe_is_open = true;
    return (0);
}

INTERNAL boolean mark_op_started
(
    rpc_socket_t        sock
)
{
    boolean ret = true;

    RPC_MUTEX_LOCK(sock->op_state_mtx);
    if (sock->free_later)
    {
        /*
         * This socket has already been closed; return false
         * to indicate that.
         */
        ret = false;
    }
    else
        sock->ops_in_progress++;
    RPC_MUTEX_UNLOCK(sock->op_state_mtx);
    return (ret);
}

INTERNAL void mark_op_finished
(
    rpc_socket_t        sock
)
{
    RPC_MUTEX_LOCK(sock->op_state_mtx);
    assert(sock->ops_in_progress != 0);
    sock->ops_in_progress--;
    if (sock->ops_in_progress == 0)
    {
        /*
         * No more operations in progress; should we free the socket?
         */
        if (sock->free_later)
        {
            /* Yes. */
            free_socket(sock);
            return;
        }
    }
    RPC_MUTEX_UNLOCK(sock->op_state_mtx);
}

/*
 * R P C _ _ S O C K E T _ S E N D
 *
 * Send data on the given socket.  An error code as well as the
 * actual number of bytes sent are returned.
 */

PRIVATE rpc_socket_error_t rpc__socket_send
(
    rpc_socket_t        sock,
    char                *data,      /* data to send */
    size_t              data_len,   /* number of bytes of data to send  */
    int                 *cc        /* returned number of bytes actually sent */
)
{
    rpc_socket_error_t serr;
    char               *p;
    off_t              offset;
    int                bytes_written;
    u_int16_t          setup[2];
    int                rparamcnt;
    int                buffer_oflow;

    if (!mark_op_started(sock))
    {
        /*
         * This socket is closed.
         */
        *cc = 0;
        serr = EBADF;
        goto done;
    }
    if (data_len > MAX_TRANSACT_SIZE) {
        /*
         * Send with WRITE_ANDX.
         * We won't get any data back in the reply to the write.
         */
        sock->trans_reply_byte_count = 0;
        sock->trans_reply_data = NULL;
        sock->must_read = true;

        p = data;
        offset = 0;
        serr = 0;
        while (data_len)
        {
            bytes_written = smb_write(sock->ctx, sock->fid, offset, data_len,
                                      p);
            if (bytes_written == -1) {
                serr = errno;
                break;
            }

            /*
             * The send is done with an ioctl, and ioctls aren't
             * necessarily cancellation points; put an explicit
             * cancellation point here.
             */
            pthread_testcancel();

            data_len -= bytes_written;
            p += bytes_written;
            offset += bytes_written;
        }
        *cc = p - data;
    }
    else
    {
        /*
         * Send with TRANSACTION.
         * We might get data back in the reply; stash it in a buffer in
	 * the rpc_socket_t structure.
         */
        setup[0] = TRANS_TRANSACT_NAMED_PIPE;
        setup[1] = sock->fid;
        rparamcnt = 0;
        sock->trans_reply_byte_count = MAX_TRANSACT_SIZE;

        serr = smb_t2_request(sock->ctx, 2, setup, "\\PIPE\\",
            0, NULL,                       /* int tparamcnt, void *tparam */
            data_len, data,                /* int tdatacnt, void *tdata */
            &rparamcnt, NULL,              /* int *rparamcnt, void *rparam */
            &sock->trans_reply_byte_count, /* int *rdatacnt */
            &sock->trans_reply_buf,        /* void *rdata */
            &buffer_oflow
        );

        /*
         * The send is done with an ioctl, and ioctls aren't
         * necessarily cancellation points; put an explicit
         * cancellation point here.
         */
        pthread_testcancel();

        sock->trans_reply_data = sock->trans_reply_buf;
        if (RPC_SOCKET_IS_ERR(serr))
        {
            *cc = 0;
        }
        else
        {
            *cc = data_len;
            if (buffer_oflow)
            {
                /*
                 * This means "there's more data coming", so we have
                 * to read it from the pipe.
                 */
                sock->must_read = true;
            }
            else
            {
                sock->must_read = false; /* nothing more to read */
            }
        }
    }

    if (!RPC_SOCKET_IS_ERR(serr))
    {
        /* We sent something, so signal the condition variable. */
        RPC_MUTEX_LOCK(sock->req_sent_mtx);
        sock->req_sent = true;
        RPC_COND_SIGNAL(sock->req_sent_cond, sock->req_sent_mtx);
        RPC_MUTEX_UNLOCK(sock->req_sent_mtx);
    }
done:
    mark_op_finished(sock);
    return (serr);
}

/*
 * R P C _ _ S O C K E T _ R E C E I V E
 *
 * Recieve the next buffer worth of information from a socket.
 * An error status as well as the actual number of bytes received
 * are also returned.
 */

PRIVATE rpc_socket_error_t rpc__socket_receive
(
    rpc_socket_t        sock,
    byte_p_t            buf,        /* buf for rcvd data */
    int                 len,        /* len of above buf */
    int                 *cc         /* returned number of bytes actually rcvd */
)
{
    rpc_socket_error_t serr;
    int                bytes_rcvd;

    if (!mark_op_started(sock))
    {
        /*
         * This socket is closed.
         */
        bytes_rcvd = 0;
        serr = EBADF;
        goto done;
    }

    /*
     * Wait until something was sent on the pipe before attempting
     * to read the reply to that something.
     */
    RPC_MUTEX_LOCK(sock->req_sent_mtx);
    while (!sock->req_sent)
    {
        /* Nothing's been sent on the pipe yet - wait until it is. */
        RPC_COND_WAIT(sock->req_sent_cond, sock->req_sent_mtx);
    }
    RPC_MUTEX_UNLOCK(sock->req_sent_mtx);

    /*
     * If the socket's been closed, give up.
     */
    if (sock->free_later)
    {
        /*
         * This socket is closed.
         */
        bytes_rcvd = 0;
        serr = EBADF;
        goto done;
    }

    /*
     * If we have data in the rpc_socket_t, process that,
     * otherwise read some from the pipe.
     */
    if (sock->trans_reply_byte_count != 0)
    {
        /*
         * We're not doing any ioctl here, so check for a
         * cancellation.
         *
         * XXX - the conditional wait above should be a
         * cancellation point.
         */
        pthread_testcancel();

        bytes_rcvd = sock->trans_reply_byte_count;
        if (bytes_rcvd > len)
            bytes_rcvd = len;
        memcpy(buf, sock->trans_reply_data, bytes_rcvd);
        sock->trans_reply_data += bytes_rcvd;
        sock->trans_reply_byte_count -= bytes_rcvd;
        serr = 0;
    }
    else
    {
    	if (!sock->must_read)
    	{
    	    /* There's nothing more to read in the reply. */
    	    bytes_rcvd = 0;
    	    serr = EIO;
    	}
    	else
    	{
            bytes_rcvd = smb_read(sock->ctx, sock->fid, 0, len, (char *)buf);
            if (bytes_rcvd == -1)
            {
                bytes_rcvd = 0;
                serr = errno;
            }
            else
                serr = 0;

            /*
             * The read is done with an ioctl, and ioctls aren't
             * necessarily cancellation points; put an explicit
             * cancellation point here.
             */
            pthread_testcancel();
	}
    }

done:
    *cc = bytes_rcvd;
    mark_op_finished(sock);
    return (serr);
}

/*
 * R P C _ _ S O C K E T _ R E C E I V E _ D O N E
 *
 * Indicate to the socket layer that we're finished receiving, in case
 * it has to know that it can't read any more until we send something
 * (as is the case with RPC-over-named-pipes).
 */

PRIVATE rpc_socket_error_t rpc__socket_receive_done
(
    rpc_socket_t        sock
)
{
    /* OK, clear the "request sent" flag for the next message. */
    RPC_MUTEX_LOCK(sock->req_sent_mtx);
    sock->req_sent = false;
    RPC_MUTEX_UNLOCK(sock->req_sent_mtx);
    return (0);
}
