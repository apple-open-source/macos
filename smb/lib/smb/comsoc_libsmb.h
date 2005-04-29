/*
 * 
 * (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
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
#ifndef _COMSOC_LIBSMB_H
#define _COMSOC_LIBSMB_H	1
/*
**
**  NAME:
**
**      comsoc_libsmb.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  The platform-specific portion of the internal network "socket" object
**  interface.  See abstract in "comsoc.h" for details.
**
*/

#if defined(__osf__) || defined(__OSF__) 
# include <sys/time.h>
#endif

/*
 * A handle to a socket.  The implementation of this type is considered
 * to be private to this package.
 */

typedef struct rpc_socket *rpc_socket_t;

/*
 * A public function for comparing two socket handles.
 */

#define RPC_SOCKET_IS_EQUAL(s1, s2)             (s1 == s2)

/*
 * This package's error type and values.  The implementation of this
 * type is considered to be private to this package.
 */

typedef int rpc_socket_error_t;                 /* a UNIX errno */

/*
 * The maximum number of iov elements which can be sent through
 * sendmsg is MSG_IOVLEN-1.
 */
#ifndef MSG_MAXIOVLEN
 /* XXX - was done only for Linux, but what about other platforms? */
 #define MSG_MAXIOVLEN 	16
#endif
#define RPC_C_MAX_IOVEC_LEN ( MSG_MAXIOVLEN - 1)

/*
 * Public error constants and functions for comparing errors.
 * The _ETOI_ (error-to-int) function converts a socket error to a simple
 * integer value that can be used in error mesages.
 */

#define RPC_C_SOCKET_OK           0             /* a successful error value */
#define RPC_C_SOCKET_EWOULDBLOCK  EWOULDBLOCK   /* operation would block */
#define RPC_C_SOCKET_EINTR        EINTR         /* operation was interrupted */
#define RPC_C_SOCKET_EIO          EIO           /* I/O error */
#define RPC_C_SOCKET_EADDRINUSE   EADDRINUSE    /* address was in use (see bind) */
#define RPC_C_SOCKET_ECONNRESET   ECONNRESET    /* connection reset by peer */
#define RPC_C_SOCKET_ETIMEDOUT    ETIMEDOUT     /* connection request timed out*/
#define RPC_C_SOCKET_ECONNREFUSED ECONNREFUSED  /* connection request refused */
#define RPC_C_SOCKET_ENOTSOCK     ENOTSOCK      /* descriptor was not a socket */
#define RPC_C_SOCKET_ENETUNREACH  ENETUNREACH   /* network is unreachable*/
#define RPC_C_SOCKET_ENOSPC       ENOSPC        /* no local or remote resources */
#define RPC_C_SOCKET_ENETDOWN     ENETDOWN      /* network is down */
#define RPC_C_SOCKET_ETOOMANYREFS ETOOMANYREFS  /* too many remote connections */
#define RPC_C_SOCKET_ESRCH        ESRCH         /* remote endpoint not found */
#define RPC_C_SOCKET_EHOSTDOWN    EHOSTDOWN     /* remote host is down */
#define RPC_C_SOCKET_EHOSTUNREACH EHOSTUNREACH  /* remote host is unreachable */
#define RPC_C_SOCKET_ECONNABORTED ECONNABORTED  /* local host aborted connect */
#define RPC_C_SOCKET_ECONNRESET   ECONNRESET    /* remote host reset connection */
#define RPC_C_SOCKET_ENETRESET    ENETRESET     /* remote host crashed */
#define RPC_C_SOCKET_ENOEXEC      ENOEXEC       /* invalid endpoint format for remote */
#define RPC_C_SOCKET_EACCESS      EACCES        /* access control information */
                                                /* invalid at remote node */
#define RPC_C_SOCKET_EPIPE        EPIPE         /* a write on a pipe */
                                                /* or socket for which there */
                                                /* is no process to */
                                                /* read the data. */
#define RPC_C_SOCKET_EAGAIN       EAGAIN        /* no more processes */
#define RPC_C_SOCKET_EALREADY     EALREADY      /* operation already */
                                                /* in progress */
#define RPC_C_SOCKET_EDEADLK      EDEADLK       /* resource deadlock */
                                                /* would occur */
#define RPC_C_SOCKET_EINPROGRESS  EINPROGRESS   /* operation now in */
                                                /* progress */
#define RPC_C_SOCKET_EISCONN      EISCONN       /* socket is already */
                                                /* connected */

/*
 * A macro to determine if an socket error can be recovered from by
 * retrying.
 */
#define RPC_SOCKET_ERR_IS_BLOCKING(s) \
    ((s == RPC_C_SOCKET_EAGAIN) || (s == RPC_C_SOCKET_EWOULDBLOCK) || (s == RPC_C_SOCKET_EINPROGRESS) || \
     (s == RPC_C_SOCKET_EALREADY) || (s == RPC_C_SOCKET_EDEADLK))

#define RPC_SOCKET_ERR_EQ(serr, e)  ((serr) == e)

#define RPC_SOCKET_IS_ERR(serr)     (! RPC_SOCKET_ERR_EQ(serr, RPC_C_SOCKET_OK))

#define RPC_SOCKET_ETOI(serr)       (serr)


#ifdef __cplusplus
extern "C" {
#endif


/*
 * R P C _ _ S O C K E T _ N E W
 *
 * Create a new socket for use with a named pipe over SMB.
 */

PRIVATE rpc_socket_error_t rpc__socket_new(
        rpc_socket_t * /*sock*/
    );


/*
 * R P C _ _ S O C K E T _ C L O S E
 *
 * Close (destroy) a socket.
 */

PRIVATE rpc_socket_error_t rpc__socket_close(
        rpc_socket_t /*sock*/
    );


/*
 * R P C _ _ S O C K E T _ C O N N E C T
 *
 * Connect a socket to a specified peer's address.
 * This is used only by Connection oriented Protocol Services.
 */

PRIVATE rpc_socket_error_t rpc__socket_connect(
        rpc_socket_t  /*sock*/,
        rpc_addr_p_t /*addr*/
    );


/*
 * R P C _ _ S O C K E T _ S E N D
 *
 * Send data on the given socket.  An error code as well as the
 * actual number of bytes sent are returned.
 */

PRIVATE rpc_socket_error_t rpc__socket_send(
        rpc_socket_t  /*sock*/,
        char *        /*data*/,     /* data to send */
        size_t        /*data_len*/, /* number of bytes of data to send  */
        int * /*cc*/                /* returned number of bytes actually sent */
    );


/*
 * R P C _ _ S O C K E T _ R E C E I V E
 *
 * Receive the next buffer worth of information from a socket.
 * An error status as well as the actual number of bytes received
 * are also returned.
 */

PRIVATE rpc_socket_error_t rpc__socket_receive(
        rpc_socket_t  /*sock*/,
        byte_p_t  /*buf*/,       /* buf for rcvd data */
        int  /*len*/,            /* len of above buf */
        int * /*cc*/             /* returned number of bytes actually rcvd */
    );



#define RPC_SOCKET_RECEIVE(sock, buf, buflen, ccp, serrp) \
    { \
        RPC_LOG_SOCKET_RECVMSG_NTR; \
        *(serrp) = rpc__socket_receive (sock, buf, buflen, ccp); \
        RPC_LOG_SOCKET_RECVMSG_XIT; \
    }

/*
 * R P C _ _ S O C K E T _ R E C E I V E _ D O N E
 *
 * Indicate to the socket layer that we're finished receiving, in case
 * it has to know that it can't read any more until we send something
 * (as is the case with RPC-over-named-pipes).
 */

PRIVATE rpc_socket_error_t rpc__socket_receive_done(
        rpc_socket_t  /*sock*/
    );

#ifdef __cplusplus
}
#endif


#define RPC_SOCKET_RECEIVE_DONE(sock, serrp) \
    { \
        *(serrp) = rpc__socket_receive_done (sock); \
    }

#endif /* _COMSOC_LIBSMB_H */
