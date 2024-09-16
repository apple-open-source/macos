/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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
**  Veneer over the BSD socket abstraction not provided by the old sock_
**  or new rpc_{tower,addr}_ components.
**
**
*/

#include <config.h>
#include <dce/lrpc.h>
#include <commonp.h>
#include <com.h>
#include <comprot.h>
#include <comnaf.h>
#include <comp.h>
#include <comsoc_bsd.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <cnp.h>

#if HAVE_LW_BASE_H
#include <lw/base.h>
#endif

#if HAVE_LWMAPSECURITY_LWMAPSECURITY_H
#include <lwmapsecurity/lwmapsecurity.h>
#endif

/* Bizarre hack for HP-UX ia64 where a system header
 * makes reference to a kernel-only data structure
 */
#if defined(__hpux) && defined(__ia64)
union mpinfou {};
#endif
#include <net/if.h>
#include <sys/ioctl.h>
/* Hack to ensure we actually get a definition of ioctl on AIX */
#if defined(_AIX) && defined(_BSD)
int ioctl(int d, int request, ...);
#endif
#include <unistd.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h> /* Not just Linux */
#endif

/*#include <dce/cma_ux_wrappers.h>*/

/* ======================================================================== */

/*
 * What we think a socket's buffering is in case rpc__socket_set_bufs()
 * fails miserably.  The #ifndef is here so that these values can be
 * overridden in a per-system file.
 */

#ifndef RPC_C_SOCKET_GUESSED_RCVBUF
#  define RPC_C_SOCKET_GUESSED_RCVBUF    (4 * 1024)
#endif

#ifndef RPC_C_SOCKET_GUESSED_SNDBUF
#  define RPC_C_SOCKET_GUESSED_SNDBUF    (4 * 1024)
#endif

/*
 * Maximum send and receive buffer sizes.  The #ifndef is here so that
 * these values can be overridden in a per-system file.
 */

#ifndef RPC_C_SOCKET_MAX_RCVBUF
#  define RPC_C_SOCKET_MAX_RCVBUF (64 * 1024)
#endif

#ifndef RPC_C_SOCKET_MAX_SNDBUF
#  define RPC_C_SOCKET_MAX_SNDBUF (64 * 1024)
#endif

/*
 * The RPC_SOCKET_DISABLE_CANCEL/RPC_SOCKET_RESTORE_CANCEL macros
 * are used to disable cancellation before entering library calls
 * which were non-cancelable under CMA threads but are generally
 * cancelable on modern POSIX systems.
 */
#define RPC_SOCKET_DISABLE_CANCEL	{ int __cs = dcethread_enableinterrupt_throw(0);
#define RPC_SOCKET_RESTORE_CANCEL	dcethread_enableinterrupt_throw(__cs); }

/*
 * Macros to paper over the difference between the 4.4bsd and 4.3bsd
 * socket API.
 *
 * The layout of a 4.4 struct sockaddr includes a 1 byte "length" field
 * which used to be one of the bytes of the "family" field.  (The "family"
 * field is now 1 byte instead of 2 bytes.)  4.4 provides binary
 * compatibility with applications compiled with a 4.3 sockaddr definition
 * by inferring a default length when the supplied length is zero.  Source
 * compatibility is blown however (if _SOCKADDR_LEN is #define'd) --
 * applications that assign only to the "family" field will leave the
 * "length" field possibly non-zero.
 *
 * Note that RPC's "sockaddr_t" is always defined to contains only a
 * family.  (We defined "rpc_addr_t" to be a struct that contains a length
 * and a sockaddr rather than mucking with the sockaddr itself.)  We
 * assumed that "sockaddr_t" and "struct sockaddr" are the same.  At
 * 4.4, this assumption caused problems.  We use RPC_SOCKET_FIX_ADDRLEN
 * at various opportunities to make sure sockaddrs' length is zero and
 * that makes the problems go away.
 *
 * ADDENDUM:
 *    This only makes the problem go away on little-endian systems
 *    where the length field on the 4.4 struct occupies the same position
 *    as the high byte of the family field on the 4.3 struct.  This is
 *    no good for i386 FreeBSD, so we have actually adapted sockaddr_t
 *    to match the system struct sockaddr.
 *                                           -- Brian Koropoff, Likewise
 *
 * RPC_SOCKET_FIX_ADDRLEN takes an "rpc_addr_p_t" (or "rpc_ip_addr_p_t")
 * as input.  The complicated casting (as opposed to simply setting
 * ".sa_len" to zero) is to ensure that the right thing happens regardless
 * of the integer endian-ness of the system).
 *
 * RPC_SOCKET_INIT_MGRHDR deals with the differences in the field names of
 * the "struct msghdr" data type between 4.3 and 4.4.
 */

#ifdef BSD_4_4_SOCKET
#define RPC_SOCKET_INIT_MSGHDR(msgp) ( \
    (msgp)->msg_control         = NULL, \
    (msgp)->msg_controllen      = 0, \
    (msgp)->msg_flags           = 0 \
)
#else
#define RPC_SOCKET_INIT_MSGHDR(msgp) ( \
    (msgp)->msg_accrights       = NULL, \
    (msgp)->msg_accrightslen    = 0 \
)
#endif /* BSD_4_4_SOCKET */

/*#if defined(_SOCKADDR_LEN)
#define RPC_SOCKET_FIX_ADDRLEN(addrp) ( \
      ((struct osockaddr *) &(addrp)->sa)->sa_family = \
              ((struct sockaddr *) &(addrp)->sa)->sa_family \
  )
  #else*/
#define RPC_SOCKET_FIX_ADDRLEN(addrp) do { } while (0)
/*#endif*/

#ifndef CMSG_ALIGN
#if defined(_CMSG_DATA_ALIGN)
#define CMSG_ALIGN _CMSG_DATA_ALIGN

#elif defined(_CMSG_ALIGN)
#define CMSG_ALIGN _CMSG_ALIGN

#elif defined(__DARWIN_ALIGN32)
#define CMSG_ALIGN __DARWIN_ALIGN32

#elif defined(ALIGN)
#define CMSG_ALIGN ALIGN
#endif
#endif /* CMSG_ALIGN */

#ifndef CMSG_SPACE
#define CMSG_SPACE(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + CMSG_ALIGN(len))
#endif

#ifndef CMSG_LEN
#define CMSG_LEN(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))
#endif

/*
 * BSD socket transport layer info structures
 */
typedef struct rpc_bsd_transport_info_s
{
    uid_t peer_uid;
    gid_t peer_gid;
} rpc_bsd_transport_info_t, *rpc_bsd_transport_info_p_t;

typedef struct rpc_bsd_socket_s
{
    int fd;
    rpc_bsd_transport_info_t info;
} rpc_bsd_socket_t, *rpc_bsd_socket_p_t;

INTERNAL rpc_socket_error_t
rpc__bsd_socket_set_default_options (
                                     rpc_socket_basic_t sockfd)
{
#ifdef SO_NOSIGPIPE
    int on = 1;

    /* Set SO_NOSIGPIPE on the socket */
    if (setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof (on)) == -1) {
        return errno;
    }
#endif
    return RPC_C_SOCKET_OK;
}

INTERNAL rpc_socket_error_t rpc__bsd_socket_destruct
(
    rpc_socket_t        sock
);

INTERNAL rpc_socket_error_t rpc__bsd_socket_construct(
    rpc_socket_t                sock,
    rpc_protseq_id_t            pseq_id,
    rpc_transport_info_handle_t info
);

/* ======================================================================== */
/*
 * R P C _ _ S O C K E T _ D U P L I C A T E
 *
 * Wrap the native socket representation in a rpc_socket_t. We duplicate the
 * socket file descriptor because we will eventually end up close(2)ing it.
 */

INTERNAL rpc_socket_error_t
rpc__bsd_socket_duplicate(
    rpc_socket_t        sock,
    rpc_protseq_id_t    pseq_id ATTRIBUTE_UNUSED,
    const void *        sockrep /* pointer to native representation */
    )
{
    rpc_socket_error_t  serr;
    rpc_bsd_socket_p_t  lrpc;
    const int *         sockfd = (const int *)sockrep;

    if (sockfd == NULL || *sockfd == -1) {
        return RPC_C_SOCKET_ENOTSOCK;
    }

    if (sock->pseq_id != pseq_id) {
        return RPC_C_SOCKET_EINVAL;
    }

    serr = rpc__bsd_socket_destruct(sock);
    if (serr != RPC_C_SOCKET_OK) {
        return serr;
    }

    serr = rpc__bsd_socket_construct(sock, pseq_id, NULL);
    if (serr != RPC_C_SOCKET_OK) {
        return serr;
    }

    lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    RPC_SOCKET_DISABLE_CANCEL;

    if (lrpc->fd != -1) {
        close(lrpc->fd);
    }

    lrpc->fd = dup(*sockfd);

    serr = ((lrpc->fd == -1) ? errno : RPC_C_SOCKET_OK);
    if (serr != RPC_C_SOCKET_OK) {
        goto error;
    }

    serr = rpc__bsd_socket_set_default_options(lrpc->fd);
    if (serr != RPC_C_SOCKET_OK) {
        goto error;
    }

    RPC_SOCKET_RESTORE_CANCEL;

    return RPC_C_SOCKET_OK;

error:
    rpc__bsd_socket_destruct(sock);
    return serr;
}

/*
 * R P C _ _ S O C K E T _ C O N S T R U C T
 *
 * Create a new socket for the specified Protocol Sequence.
 * The new socket has blocking IO semantics.
 *
 * (see BSD UNIX socket(2)).
 */

INTERNAL rpc_socket_error_t
rpc__bsd_socket_construct(
    rpc_socket_t sock,
    rpc_protseq_id_t    pseq_id,
    rpc_transport_info_handle_t info ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t  serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = NULL;

    /* set data.point to be 0 in case we get an error
     and have to call rpc__bsd_socket_destruct */
    sock->data.pointer = NULL;

    lrpc = calloc(1, sizeof(*lrpc));

    if (!lrpc)
    {
        serr = ENOMEM;
        goto error;
    }

    sock->data.pointer = (void*) lrpc;

    lrpc->fd            = -1;
    lrpc->info.peer_uid = -1;
    lrpc->info.peer_gid = -1;

    RPC_SOCKET_DISABLE_CANCEL;
    lrpc->fd = socket(
        (int) RPC_PROTSEQ_INQ_NAF_ID(pseq_id),
        (int) RPC_PROTSEQ_INQ_NET_IF_ID(pseq_id),
        0 /*(int) RPC_PROTSEQ_INQ_NET_PROT_ID(pseq_id)*/);
    serr = ((lrpc->fd == -1) ? errno : RPC_C_SOCKET_OK);

    if (serr == RPC_C_SOCKET_OK) {
        serr = rpc__bsd_socket_set_default_options(lrpc->fd);
        if (serr != RPC_C_SOCKET_OK)
            close (lrpc->fd);
    }
    RPC_SOCKET_RESTORE_CANCEL;

    if (serr)
    {
        goto error;
    }

done:
    return serr;

error:
    if (lrpc)
    {
        rpc__bsd_socket_destruct(sock);
    }

    goto done;
}

/*
 * R P C _ _ S O C K E T _ O P E N _ B A S I C
 *
 * A special version of socket_open that is used *only* by
 * the low level initialization code when it is trying to
 * determine what network services are supported by the host OS.
 */

PRIVATE rpc_socket_error_t
rpc__bsd_socket_open_basic(
    rpc_naf_id_t        naf,
    rpc_network_if_id_t net_if,
    rpc_network_protocol_id_t net_prot ATTRIBUTE_UNUSED,
    rpc_socket_basic_t        *sock
    )
{
    rpc_socket_error_t  serr;

    /*
     * Always pass zero as socket protocol to compensate for
     * overloading the protocol field for named pipes
     */
    RPC_SOCKET_DISABLE_CANCEL;
    *sock = socket((int) naf, (int) net_if, 0);
    serr = ((*sock == -1) ? errno : RPC_C_SOCKET_OK);

    if (serr == RPC_C_SOCKET_OK) {
        serr = rpc__bsd_socket_set_default_options(*sock);
        if (serr != RPC_C_SOCKET_OK)
            close (*sock);
    }
    RPC_SOCKET_RESTORE_CANCEL;

    return serr;
}

PRIVATE rpc_socket_error_t
rpc__bsd_socket_close_basic(
    rpc_socket_basic_t        sock
    )
{
    rpc_socket_error_t  serr;

    RPC_LOG_SOCKET_CLOSE_NTR;
    RPC_SOCKET_DISABLE_CANCEL;
    serr = (close(sock) == -1) ? errno : RPC_C_SOCKET_OK;
    RPC_SOCKET_RESTORE_CANCEL;
    RPC_LOG_SOCKET_CLOSE_XIT;

    return (serr);
}


/*
 * R P C _ _ S O C K E T _ C L O S E
 *
 * Close (destroy) a socket.
 *
 * (see BSD UNIX close(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_destruct
(
    rpc_socket_t        sock
)
{
    rpc_socket_error_t  serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    if (lrpc && lrpc->fd > 0)
    {

        RPC_LOG_SOCKET_CLOSE_NTR;
        RPC_SOCKET_DISABLE_CANCEL;
        serr = (close(lrpc->fd) == -1) ? errno : RPC_C_SOCKET_OK;
        RPC_SOCKET_RESTORE_CANCEL;
        RPC_LOG_SOCKET_CLOSE_XIT;
    }

    if (lrpc)
    {
	free(lrpc);
        sock->data.pointer = NULL;
    }

    return serr;
}

/*
 * R P C _ _ S O C K E T _ B I N D
 *
 * Bind a socket to a specified local address.
 *
 * (see BSD UNIX bind(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_bind
(
    rpc_socket_t        sock,
    rpc_addr_p_t        addr
)
{
    rpc_socket_error_t  serr = EINVAL;
    unsigned32 status;
    rpc_addr_p_t temp_addr = NULL;
    boolean has_endpoint = false;
    int setsock_val = 1;
    int ncalrpc;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    RPC_LOG_SOCKET_BIND_NTR;

    ncalrpc = addr->rpc_protseq_id == rpc_c_protseq_id_ncalrpc;

    /*
     * Check if the address has a well-known endpoint.
     */
    if (addr->rpc_protseq_id == rpc_c_protseq_id_ncacn_ip_tcp || ncalrpc)
    {
        unsigned_char_t *endpoint;

        rpc__naf_addr_inq_endpoint (addr, &endpoint, &status);

        if (status == rpc_s_ok && endpoint != NULL)
        {
            if (endpoint[0] != '\0')    /* test for null string */
                has_endpoint = true;

            rpc_string_free (&endpoint, &status);
        }
        status = rpc_s_ok;
    }

    /*
     * If there is no port restriction in this address family, then do a
     * simple bind.
     */

    if (! RPC_PROTSEQ_TEST_PORT_RESTRICTION (addr -> rpc_protseq_id))
    {
        if (!has_endpoint && ncalrpc)
        {
            serr = 0;
        }
        else
        {
#if defined(SOL_SOCKET) && defined(SO_REUSEADDR)
	    setsockopt(lrpc->fd, SOL_SOCKET, SO_REUSEADDR,
		       &setsock_val, sizeof(setsock_val));
#endif
            if (addr->sa.family == AF_UNIX && addr->sa.data[0] != '\0')
            {
                // This function is going to bind a named socket. First, try
                // to delete the path incase a previous instance of a program
                // left it behind.
                //
                // Ignore any errors from this function.
                unlink((const char*)addr->sa.data);
            }
            serr =
                (bind(lrpc->fd, (struct sockaddr *)&addr->sa, addr->len) == -1) ?
                      errno : RPC_C_SOCKET_OK;
        }
    }                                   /* no port restriction */

    else
    {
        /*
         * Port restriction is in place.  If the address has a well-known
         * endpoint, then do a simple bind.
         */

        if (has_endpoint)
        {
#if defined(SOL_SOCKET) && defined(SO_REUSEADDR)
	    setsockopt(lrpc->fd, SOL_SOCKET, SO_REUSEADDR,
		       &setsock_val, sizeof(setsock_val));
#endif
            serr = (bind(lrpc->fd, (struct sockaddr *)&addr->sa, addr->len) == -1)?
                errno : RPC_C_SOCKET_OK;
        }                               /* well-known endpoint */
        else
	{

	    unsigned_char_t *endpoint;
	    unsigned char c;

	    rpc__naf_addr_inq_endpoint (addr, &endpoint, &status);

	    c = endpoint[0];               /* grab first char */
	    rpc_string_free (&endpoint, &status);

	    if (c != '\0')       /* test for null string */
	    {
	        serr = (bind(lrpc->fd, (struct sockaddr *)&addr->sa, addr->len) == -1)?
		    errno : RPC_C_SOCKET_OK;
	    }                               /* well-known endpoint */

	    else
	    {
	        /*
	         * Port restriction is in place and the address doesn't have a
	         * well-known endpoint.  Try to bind until we hit a good port,
	         * or exhaust the retry count.
	         *
	         * Make a copy of the address to work in; if we hardwire an
	         * endpoint into our caller's address, later logic could infer
	         * that it is a well-known endpoint.
	         */

	        unsigned32 i;
	        boolean found;

	        for (i = 0, found = false;
		     (i < RPC_PORT_RESTRICTION_INQ_N_TRIES (addr->rpc_protseq_id))
		     && !found;
		     i++)
	        {
		    unsigned_char_p_t port_name;

		    rpc__naf_addr_overcopy (addr, &temp_addr, &status);

		    if (status != rpc_s_ok)
		    {
		        serr = RPC_C_SOCKET_EIO;
		        break;
		    }

		    rpc__naf_get_next_restricted_port (temp_addr -> rpc_protseq_id,
						   &port_name, &status);

		    if (status != rpc_s_ok)
		    {
		        serr = RPC_C_SOCKET_EIO;
		        break;
		    }

		    rpc__naf_addr_set_endpoint (port_name, &temp_addr, &status);

		    if (status != rpc_s_ok)
		    {
		        serr = RPC_C_SOCKET_EIO;
		        rpc_string_free (&port_name, &status);
		        break;
		    }

		    if (bind(lrpc->fd, (struct sockaddr *)&temp_addr->sa, temp_addr->len) == 0)
		    {
		        found = true;
		        serr = RPC_C_SOCKET_OK;
		    }
		    else
		        serr = RPC_C_SOCKET_EIO;

		    rpc_string_free (&port_name, &status);
	        }                           /* for i */

	        if (!found)
	        {
		    serr = RPC_C_SOCKET_EADDRINUSE;
	        }
	    }                               /* no well-known endpoint */
        }				/* has endpoint */
    }                                   /* port restriction is in place */

    if (serr == RPC_C_SOCKET_OK && ncalrpc && has_endpoint)
    {
	struct sockaddr_un *skun = (struct sockaddr_un *)&addr->sa;

	serr = chmod(skun->sun_path,
		     S_IRUSR | S_IWUSR | S_IXUSR |
		     S_IRGRP | S_IWGRP | S_IXGRP |
		     S_IROTH | S_IWOTH | S_IXOTH) == -1 ? errno : RPC_C_SOCKET_OK;
    }

    if (temp_addr != NULL)
        rpc__naf_addr_free (&temp_addr, &status);

    RPC_LOG_SOCKET_BIND_XIT;
    return (serr);
}

INTERNAL rpc_socket_error_t rpc__bsd_socket_getpeereid
(
    rpc_socket_t        sock,
    uid_t		*euid,
    gid_t		*egid
);

#if !defined(SO_PEERCRED) && !defined(HAVE_GETPEEREID)

INTERNAL rpc_socket_error_t rpc__bsd_socket_sendpeereid
(
    rpc_socket_t        sock,
    rpc_addr_p_t        addr
);

INTERNAL rpc_socket_error_t rpc__bsd_socket_recvpeereid
(
    rpc_socket_t        sock,
    uid_t		*euid,
    gid_t		*egid
);

#endif


/*
 * R P C _ _ S O C K E T _ C O N N E C T
 *
 * Connect a socket to a specified peer's address.
 * This is used only by Connection oriented Protocol Services.
 *
 * (see BSD UNIX connect(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_connect
(
    rpc_socket_t        sock,
    rpc_addr_p_t        addr,
    rpc_cn_assoc_t      *assoc ATTRIBUTE_UNUSED
)
{
    rpc_socket_error_t  serr;
    //rpc_binding_rep_t *binding_rep;
    unsigned_char_t *netaddr, *endpoint;
    unsigned32      dbg_status;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    rpc__naf_addr_inq_netaddr (addr,
                               &netaddr,
                               &dbg_status);
    rpc__naf_addr_inq_endpoint (addr,
                               &endpoint,
                               &dbg_status);

    RPC_DBG_PRINTF (rpc_e_dbg_general, RPC_C_CN_DBG_GENERAL,
        ("CN: connection request initiated to %s[%s]\n",
         netaddr,
         endpoint));

connect_again:
    RPC_LOG_SOCKET_CONNECT_NTR;
    serr = (connect (
                (int) lrpc->fd,
                (struct sockaddr *) (&addr->sa),
                (int) (addr->len))
            == -1) ? errno : RPC_C_SOCKET_OK;
    RPC_LOG_SOCKET_CONNECT_XIT;
    if (serr == EINTR)
    {
        goto connect_again;
    }
    else if (serr != RPC_C_SOCKET_OK)
    {
        goto error;
    }

#if !defined(SO_PEERCRED) && !defined(HAVE_GETPEEREID)
    serr = rpc__bsd_socket_sendpeereid(sock, addr);
#endif

cleanup:
    rpc_string_free (&netaddr, &dbg_status);
    rpc_string_free (&endpoint, &dbg_status);

    return serr;

error:
    goto cleanup;
}

/*
 * R P C _ _ S O C K E T _ A C C E P T
 *
 * Accept a connection on a socket, creating a new socket for the new
 * connection.  A rpc_addr_t appropriate for the NAF corresponding to
 * this socket must be provided.  addr.len must set to the actual size
 * of addr.sa.  This operation fills in addr.sa and sets addr.len to
 * the new size of the field.  This is used only by Connection oriented
 * Protocol Services.
 *
 * (see BSD UNIX accept(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_accept
(
    rpc_socket_t        sock,
    rpc_addr_p_t        addr,
    rpc_socket_t        *newsock
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;
    rpc_bsd_socket_p_t newlrpc = NULL;
    boolean            isUnixDomainSocket = false;
    uid_t euid = -1;
    gid_t egid = -1;

    *newsock = malloc(sizeof (**newsock));

    if (!*newsock)
    {
        return ENOMEM;
    }

    (*newsock)->vtbl = sock->vtbl;
    (*newsock)->pseq_id = sock->pseq_id;

    newlrpc = malloc(sizeof(*newlrpc));
    if (!newlrpc)
    {
        return ENOMEM;
    }

    newlrpc->info.peer_uid = -1;
    newlrpc->info.peer_gid = -1;

    (*newsock)->data.pointer = newlrpc;

accept_again:
    RPC_LOG_SOCKET_ACCEPT_NTR;
    if (addr == NULL)
    {
        struct sockaddr_storage tmpSockAddr = {0};
        socklen_t addrlen;

        addrlen = sizeof(tmpSockAddr);;
        newlrpc->fd = accept
            ((int) lrpc->fd, (struct sockaddr *) &tmpSockAddr, &addrlen);
        
        if (newlrpc->fd != -1) {
            isUnixDomainSocket = (tmpSockAddr.ss_family == AF_UNIX) ? true : false;
        }
    }
    else
    {
        newlrpc->fd = accept
            ((int) lrpc->fd, (struct sockaddr *) (&addr->sa), (&addr->len));
        
        if (newlrpc->fd != -1) {
            isUnixDomainSocket = (addr->sa.family == AF_UNIX) ? true : false;
        }
    }
    serr = (newlrpc->fd == -1) ? errno : RPC_C_SOCKET_OK;
    RPC_LOG_SOCKET_ACCEPT_XIT;

    if (!serr)
    {
        if (isUnixDomainSocket == true) {
            serr = rpc__bsd_socket_getpeereid((*newsock), &euid, &egid);
        }
    }
    else
    {
        goto cleanup;
    }

    if (serr == EINTR)
    {
        goto accept_again;
    }

    if (!(serr) && (isUnixDomainSocket == true)) {
        newlrpc->info.peer_uid = euid;
        newlrpc->info.peer_gid = egid;
    }

cleanup:
    if (serr && newlrpc)
    {
        free(newlrpc);
    }

    if (serr && *newsock)
    {
        free(*newsock);
    }

    return serr;
}

/*
 * R P C _ _ S O C K E T _ L I S T E N
 *
 * Listen for a connection on a socket.
 * This is used only by Connection oriented Protocol Services.
 *
 * (see BSD UNIX listen(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_listen
(
    rpc_socket_t        sock,
    int                 backlog
)
{
    rpc_socket_error_t  serr;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    RPC_LOG_SOCKET_LISTEN_NTR;
    RPC_SOCKET_DISABLE_CANCEL;
    serr = (listen(lrpc->fd, backlog) == -1) ? errno : RPC_C_SOCKET_OK;
    RPC_SOCKET_RESTORE_CANCEL;
    RPC_LOG_SOCKET_LISTEN_XIT;
    return (serr);
}

/*
 * R P C _ _ S O C K E T _ S E N D M S G
 *
 * Send a message over a given socket.  An error code as well as the
 * actual number of bytes sent are returned.
 *
 * (see BSD UNIX sendmsg(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_sendmsg
(
    rpc_socket_t        sock,
    rpc_socket_iovec_p_t iov,       /* array of bufs of data to send */
    int                 iovcnt,     /* number of bufs */
    rpc_addr_p_t        addr,       /* addr of receiver */
    size_t              *cc         /* returned number of bytes actually sent */
)
{
    ssize_t             ret;
    rpc_socket_error_t  serr;
    struct msghdr       msg;
    rpc_bsd_socket_p_t  lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    *cc = 0;

sendmsg_again:

    memset(&msg, 0, sizeof(msg));
    RPC_LOG_SOCKET_SENDMSG_NTR;
    RPC_SOCKET_INIT_MSGHDR(&msg);
    if ((addr) != NULL)
    {
        RPC_SOCKET_FIX_ADDRLEN(addr);
        msg.msg_name = (caddr_t) &(addr)->sa;
        msg.msg_namelen = (addr)->len;
    }
    else
    {
        msg.msg_name = (caddr_t) NULL;
    }
    msg.msg_iov = iov;
    msg.msg_iovlen = iovcnt;

    ret = dcethread_sendmsg (lrpc->fd, &msg, 0);
    if (ret == (ssize_t) -1)
    {
        serr = errno;
    }
    else
    {
        serr = RPC_C_SOCKET_OK;
        *cc = ret;
    }

    RPC_LOG_SOCKET_SENDMSG_XIT;
    if (serr == EINTR)
    {
        goto sendmsg_again;
    }

    return (serr);
}

/*
 * R P C _ _ S O C K E T _ R E C V F R O M
 *
 * Recieve the next buffer worth of information from a socket.  A
 * rpc_addr_t appropriate for the NAF corresponding to this socket must
 * be provided.  addr.len must set to the actual size of addr.sa.  This
 * operation fills in addr.sa and sets addr.len to the new size of the
 * field.  An error status as well as the actual number of bytes received
 * are also returned.
 *
 * (see BSD UNIX recvfrom(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_recvfrom
(
    rpc_socket_t        sock,
    byte_p_t            buf,        /* buf for rcvd data */
    int                 len,        /* len of above buf */
    rpc_addr_p_t        from,       /* addr of sender */
    size_t              *cc         /* returned number of bytes actually rcvd */
)
{
    ssize_t             ret;
    rpc_socket_error_t  serr;
    rpc_bsd_socket_p_t  lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    *cc = 0;

recvfrom_again:
    if (from != NULL)
    {
        RPC_SOCKET_FIX_ADDRLEN(from);
    }

    RPC_LOG_SOCKET_RECVFROM_NTR;

    ret = dcethread_recvfrom(lrpc->fd, buf, len, 0 /* flags */,
                    (struct sockaddr *) &from->sa, &from->len);
    if (ret == (ssize_t) -1)
    {
        serr = errno;
    }
    else
    {
        serr = RPC_C_SOCKET_OK;
        *cc = ret;
    }

    RPC_LOG_SOCKET_RECVFROM_XIT;
    RPC_SOCKET_FIX_ADDRLEN(from);
    if (serr == EINTR)
    {
        goto recvfrom_again;
    }

    return serr;
}

/*
 * R P C _ _ S O C K E T _ R E C V M S G
 *
 * Receive a message over a given socket.  A rpc_addr_t appropriate for
 * the NAF corresponding to this socket must be provided.  addr.len must
 * set to the actual size of addr.sa.  This operation fills in addr.sa
 * and sets addr.len to the new size of the field.  An error code as
 * well as the actual number of bytes received are also returned.
 *
 * (see BSD UNIX recvmsg(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_recvmsg
(
    rpc_socket_t        sock,
    rpc_socket_iovec_p_t iov,       /* array of bufs for rcvd data */
    int                 iovcnt,    /* number of bufs */
    rpc_addr_p_t        addr,       /* addr of sender */
    size_t              *cc         /* returned number of bytes actually rcvd */
)
{
    ssize_t             ret;
    rpc_socket_error_t  serr;
    struct msghdr       msg;
    rpc_bsd_socket_p_t  lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;
    int block = 0;

    *cc = 0;

recvmsg_again:

    memset(&msg, 0, sizeof(msg));
    RPC_LOG_SOCKET_RECVMSG_NTR;
    RPC_SOCKET_INIT_MSGHDR(&msg);
    if (addr != NULL)
    {
        RPC_SOCKET_FIX_ADDRLEN(addr);
        msg.msg_name = (caddr_t) &addr->sa;
        msg.msg_namelen = addr->len;
    }
    else
    {
        msg.msg_name = (caddr_t) NULL;
    }
    msg.msg_iov =  iov;
    msg.msg_iovlen = iovcnt;

    /*
     * WORKAROUND :
     * - Inconsistency between kernel socket state and socket file descriptor state -
     * fcntl(F_GETFL) here tells us that socket is blocking (O_NONBLOCK not set in flags),
     * however kernel dtrace probe for soreceive says socket is SS_NBIO.
     * See 18110085 / 17781539 for details.
     * Setting FIONBIO to 0 from userland will resolve this incosistency and make both
     * socket state and socket file descriptor state in the kernel land, and gurantees a
     * blocking socket before recvmsg() is called.
     */
    ioctl(lrpc->fd, FIONBIO, &block);

    ret = dcethread_recvmsg (lrpc->fd, &msg, 0);
    if (ret == (ssize_t)-1)
    {
        serr = errno;
    }
    else
    {
        serr = RPC_C_SOCKET_OK;
        *cc = ret;
    }

    RPC_LOG_SOCKET_RECVMSG_XIT;
    if (serr == EINTR)
    {
        goto recvmsg_again;
    }

    if (addr != NULL)
    {
        addr->len = msg.msg_namelen;
    }

    return serr;
}

/*
 * R P C _ _ S O C K E T _ I N Q _ A D D R
 *
 * Return the local address associated with a socket.  A rpc_addr_t
 * appropriate for the NAF corresponding to this socket must be provided.
 * addr.len must set to the actual size of addr.sa.  This operation fills
 * in addr.sa and sets addr.len to the new size of the field.
 *
 * !!! NOTE: You should use rpc__naf_desc_inq_addr() !!!
 *
 * This routine is indended for use only by the internal routine:
 * rpc__naf_desc_inq_addr().  rpc__bsd_socket_inq_endpoint() only has the
 * functionality of BSD UNIX getsockname() which doesn't (at least not
 * on all systems) return the local network portion of a socket's address.
 * rpc__naf_desc_inq_addr() returns the complete address for a socket.
 *
 * (see BSD UNIX getsockname(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_inq_endpoint
(
    rpc_socket_t        sock,
    rpc_addr_p_t        addr
)
{
    rpc_socket_error_t  serr;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    RPC_LOG_SOCKET_INQ_EP_NTR;
    RPC_SOCKET_FIX_ADDRLEN(addr);
    RPC_SOCKET_DISABLE_CANCEL;
    serr = (getsockname(lrpc->fd, (void*)&addr->sa, &addr->len) == -1) ? errno : RPC_C_SOCKET_OK;
    RPC_SOCKET_RESTORE_CANCEL;
    RPC_SOCKET_FIX_ADDRLEN(addr);
    RPC_LOG_SOCKET_INQ_EP_XIT;
    return (serr);
}

/*
 * R P C _ _ S O C K E T _ S E T _ B R O A D C A S T
 *
 * Enable broadcasting for the socket (as best it can).
 * Used only by Datagram based Protocol Services.
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_set_broadcast
(
    rpc_socket_t        sock
)
{
#ifdef SO_BROADCAST
    int                 setsock_val = 1;
    rpc_socket_error_t  serr;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    RPC_SOCKET_DISABLE_CANCEL;
    serr = (setsockopt(lrpc->fd, SOL_SOCKET, SO_BROADCAST,
            &setsock_val, sizeof(setsock_val)) == -1) ? errno : RPC_C_SOCKET_OK;
    RPC_SOCKET_RESTORE_CANCEL;
    if (serr)
    {
        RPC_DBG_GPRINTF(("(rpc__bsd_socket_set_broadcast) error=%d\n", serr));
    }

    return(serr);
#else
    return(RPC_C_SOCKET_OK);
#endif
}

/*
 * R P C _ _ S O C K E T _ S E T _ B U F S
 *
 * Set the socket's send and receive buffer sizes and return the new
 * values.  Note that the sizes are min'd with
 * "rpc_c_socket_max_{snd,rcv}buf" because systems tend to fail the
 * operation rather than give the max buffering if the max is exceeded.
 *
 * If for some reason your system is screwed up and defines SOL_SOCKET
 * and SO_SNDBUF, but doesn't actually support the SO_SNDBUF and SO_RCVBUF
 * operations AND using them would result in nasty behaviour (i.e. they
 * don't just return some error code), define NO_SO_SNDBUF.
 *
 * If the buffer sizes provided are 0, then we use the operating
 * system default (i.e. we don't set anything at all).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_set_bufs
(
    rpc_socket_t        sock,
    unsigned32          txsize,
    unsigned32          rxsize,
    unsigned32          *ntxsize,
    unsigned32          *nrxsize
)
{
    socklen_t sizelen;
    int e;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    RPC_SOCKET_DISABLE_CANCEL;

#if (defined (SOL_SOCKET) && defined(SO_SNDBUF)) && !defined(NO_SO_SNDBUF)

    /*
     * Set the new sizes.
     */

    txsize = MIN(txsize, RPC_C_SOCKET_MAX_SNDBUF);
    if (txsize != 0)
    {
        e = setsockopt(lrpc->fd, SOL_SOCKET, SO_SNDBUF, &txsize, sizeof(txsize));
        if (e == -1)
        {
            RPC_DBG_GPRINTF
                (("(rpc__bsd_socket_set_bufs) WARNING: set sndbuf (%d) failed - error = %d\n",
                txsize, errno));
        }
    }

    rxsize = MIN(rxsize, RPC_C_SOCKET_MAX_RCVBUF);
    if (rxsize != 0)
    {
        e = setsockopt(lrpc->fd, SOL_SOCKET, SO_RCVBUF, &rxsize, sizeof(rxsize));
        if (e == -1)
        {
            RPC_DBG_GPRINTF
                (("(rpc__bsd_socket_set_bufs) WARNING: set rcvbuf (%d) failed - error = %d\n",
                rxsize, errno));
        }
    }

    /*
     * Get the new sizes.  If this fails, just return some guessed sizes.
     */
    *ntxsize = 0;
    sizelen = sizeof *ntxsize;
    e = getsockopt(lrpc->fd, SOL_SOCKET, SO_SNDBUF, ntxsize, &sizelen);
    if (e == -1)
    {
        RPC_DBG_GPRINTF
            (("(rpc__bsd_socket_set_bufs) WARNING: get sndbuf failed - error = %d\n", errno));
        *ntxsize = RPC_C_SOCKET_GUESSED_SNDBUF;
    }

    *nrxsize = 0;
    sizelen = sizeof *nrxsize;
    e = getsockopt(lrpc->fd, SOL_SOCKET, SO_RCVBUF, nrxsize, &sizelen);
    if (e == -1)
    {
        RPC_DBG_GPRINTF
            (("(rpc__bsd_socket_set_bufs) WARNING: get rcvbuf failed - error = %d\n", errno));
        *nrxsize = RPC_C_SOCKET_GUESSED_RCVBUF;
    }

#  ifdef apollo
    /*
     * On Apollo, modifying the socket buffering doesn't actually do
     * anything on IP sockets, but the calls succeed anyway.  We can
     * detect this by the fact that the new buffer length returned is
     * 0. Return what we think the actually length is.
     */
    if (rxsize != 0 && *nrxsize == 0)
    {
        *nrxsize = (8 * 1024);
    }
    if (txsize != 0 && *ntxsize == 0)
    {
        *ntxsize = (8 * 1024);
    }
#  endif

#else

    *ntxsize = RPC_C_SOCKET_GUESSED_SNDBUF;
    *nrxsize = RPC_C_SOCKET_GUESSED_RCVBUF;

#endif

    RPC_SOCKET_RESTORE_CANCEL;

    return (RPC_C_SOCKET_OK);
}

/*
 * R P C _ _ S O C K E T _ S E T _ N B I O
 *
 * Set a socket to non-blocking mode.
 *
 * Return RPC_C_SOCKET_OK on success, otherwise an error value.
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_set_nbio
(
    rpc_socket_t        sock
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    RPC_SOCKET_DISABLE_CANCEL;
    serr = ((fcntl(lrpc->fd, F_SETFL, O_NDELAY) == -1) ? errno : RPC_C_SOCKET_OK);
    RPC_SOCKET_RESTORE_CANCEL;
    if (serr)
    {
        RPC_DBG_GPRINTF(("(rpc__bsd_socket_set_nbio) error=%d\n", serr));
    }

    return (serr);
}

/*
 * R P C _ _ S O C K E T _ S E T _ C L O S E _ O N _ E X E C
 *
 *
 * Set a socket to a mode whereby it is not inherited by a spawned process
 * executing some new image. This is possibly a no-op on some systems.
 *
 * Return RPC_C_SOCKET_OK on success, otherwise an error value.
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_set_close_on_exec
(
    rpc_socket_t        sock
)
{
    rpc_socket_error_t  serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    RPC_SOCKET_DISABLE_CANCEL;
    serr = ((fcntl(lrpc->fd, F_SETFD, 1) == -1) ? errno : RPC_C_SOCKET_OK);
    RPC_SOCKET_RESTORE_CANCEL;
    if (serr)
    {
        RPC_DBG_GPRINTF(("(rpc__bsd_socket_set_close_on_exec) error=%d\n", serr));
    }
    return (serr);
}

/*
 * R P C _ _ S O C K E T _ G E T P E E R N A M E
 *
 * Get name of connected peer.
 * This is used only by Connection oriented Protocol Services.
 *
 * (see BSD UNIX getpeername(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_getpeername
(
    rpc_socket_t sock,
    rpc_addr_p_t addr
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    RPC_SOCKET_FIX_ADDRLEN(addr);
    RPC_SOCKET_DISABLE_CANCEL;
    serr = (getpeername(lrpc->fd, (void *)&addr->sa, &addr->len) == -1) ? errno : RPC_C_SOCKET_OK;
    RPC_SOCKET_RESTORE_CANCEL;
    RPC_SOCKET_FIX_ADDRLEN(addr);

    return (serr);
}

/*
 * R P C _ _ S O C K E T _ G E T _ I F _ I D
 *
 * Get socket network interface id (socket type).
 *
 * (see BSD UNIX getsockopt(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_get_if_id
(
    rpc_socket_t        sock,
    rpc_network_if_id_t *network_if_id
)
{
    socklen_t optlen = 0;
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    optlen = sizeof(rpc_network_if_id_t);

    RPC_SOCKET_DISABLE_CANCEL;
    serr = (getsockopt (lrpc->fd,
                        SOL_SOCKET,
                        SO_TYPE,
                        network_if_id,
                        &optlen) == -1  ? errno : RPC_C_SOCKET_OK);
    RPC_SOCKET_RESTORE_CANCEL;
    return serr;
}

/*
 * R P C _ _ S O C K E T _ S E T _ K E E P A L I V E
 *
 * Enable periodic transmissions on a connected socket, when no
 * other data is being exchanged. If the other end does not respond to
 * these messages, the connection is considered broken and the
 * so_error variable is set to ETIMEDOUT.
 * Used only by Connection based Protocol Services.
 *
 * (see BSD UNIX setsockopt(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_set_keepalive
(
    rpc_socket_t        sock
)
{
#ifdef SO_KEEPALIVE
    int                 setsock_val = 1;
    rpc_socket_error_t  serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    RPC_SOCKET_DISABLE_CANCEL;
    serr = ((setsockopt(lrpc->fd, SOL_SOCKET, SO_KEEPALIVE,
       &setsock_val, sizeof(setsock_val)) == -1) ? errno : RPC_C_SOCKET_OK);
    RPC_SOCKET_RESTORE_CANCEL;
    if (serr)
    {
        RPC_DBG_GPRINTF(("(rpc__bsd_socket_set_keepalive) error=%d\n", serr));
    }

    return(serr);
#else
    return(RPC_C_SOCKET_OK);
#endif
}


/*
 * R P C _ _ S O C K E T _ N O W R I T E B L O C K _ W A I T
 *
 * Wait until the a write on the socket should succede without
 * blocking.  If tmo is NULL, the wait is unbounded, otherwise
 * tmo specifies the max time to wait. RPC_C_SOCKET_ETIMEDOUT
 * if a timeout occurs.  This operation in not cancellable.
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_nowriteblock_wait
(
    rpc_socket_t sock,
    struct timeval *tmo
)
{
    fd_set  write_fds;
    int     nfds, num_found;
    rpc_socket_error_t  serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    FD_ZERO (&write_fds);
    FD_SET (lrpc->fd, &write_fds);
    nfds = lrpc->fd + 1;

    RPC_SOCKET_DISABLE_CANCEL;
    num_found = dcethread_select(nfds, NULL, (void *)&write_fds, NULL, tmo);
    serr = ((num_found < 0) ? errno : RPC_C_SOCKET_OK);
    RPC_SOCKET_RESTORE_CANCEL;

    if (serr)
    {
        RPC_DBG_GPRINTF(("(rpc__bsd_socket_nowriteblock_wait) error=%d\n", serr));
        return serr;
    }

    if (num_found == 0)
    {
        RPC_DBG_GPRINTF(("(rpc__bsd_socket_nowriteblock_wait) timeout\n"));
        return RPC_C_SOCKET_ETIMEDOUT;
    }

    return RPC_C_SOCKET_OK;
}


/*
 * R P C _ _ S O C K E T _ S E T _ R C V T I M E O
 *
 * Set receive timeout on a socket
 * Used only by Connection based Protocol Services.
 *
 * (see BSD UNIX setsockopt(2)).
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_set_rcvtimeo
(
    rpc_socket_t        sock,
    struct timeval      *tmo
)
{
#ifdef SO_RCVTIMEO
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    RPC_SOCKET_DISABLE_CANCEL;
    serr = ((setsockopt(lrpc->fd, SOL_SOCKET, SO_RCVTIMEO,
       tmo, sizeof(*tmo)) == -1) ? errno : RPC_C_SOCKET_OK);
    RPC_SOCKET_RESTORE_CANCEL;
    if (serr)
    {
        RPC_DBG_GPRINTF(("(rpc__bsd_socket_set_rcvtimeo) error=%d\n", serr));
    }

    return(serr);
#else
    return(RPC_C_SOCKET_OK);
#endif
}

/*
 * R P C _ _ S O C K E T _ G E T _ P E E R E I D
 *
 * Get UNIX domain socket peer credentials
 */

INTERNAL rpc_socket_error_t rpc__bsd_socket_getpeereid
(
    rpc_socket_t        sock,
    uid_t		*euid,
    gid_t		*egid
)
{
    rpc_socket_error_t  serr = RPC_C_SOCKET_ENOTSUP;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

#if HAVE_GETPEEREID

    RPC_SOCKET_DISABLE_CANCEL;
    serr = ((getpeereid(lrpc->fd, euid, egid) == -1) ? errno
                                            : RPC_C_SOCKET_OK);
    RPC_SOCKET_RESTORE_CANCEL;

    if (serr != RPC_C_SOCKET_OK)
    {
        RPC_DBG_GPRINTF(("(rpc__bsd_socket_getpeereid) error=%d\n", serr));
    }

#elif defined(SO_PEERCRED)
    struct ucred peercred = {0};
    socklen_t peercredlen = sizeof(peercred);

    RPC_SOCKET_DISABLE_CANCEL;
    serr = ((getsockopt(lrpc->fd, SOL_SOCKET, SO_PEERCRED,
	&peercred, &peercredlen) == -1) ? errno : RPC_C_SOCKET_OK);
    RPC_SOCKET_RESTORE_CANCEL;
    if (serr == RPC_C_SOCKET_OK)
    {
	*euid = peercred.uid;
	*egid = peercred.gid;
    }
    else
    {
        RPC_DBG_GPRINTF(("(rpc__bsd_socket_getpeereid) error=%d\n", serr));
    }
#else
    serr = rpc__bsd_socket_recvpeereid(sock, euid, egid);
#endif

    return serr;
}

#if !defined(SO_PEERCRED) && !defined(HAVE_GETPEEREID)

INTERNAL rpc_socket_error_t rpc__bsd_socket_sendpeereid
(
    rpc_socket_t        sock,
    rpc_addr_p_t        addr
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;
    struct sockaddr_un *endpoint_addr = NULL;
    struct stat endpoint_stat = {0};
    uid_t ep_uid = -1;
    int pipefd[2] = {-1, -1};
    char empty_buf[] = {'\0'};
    rpc_socket_iovec_t iovec = {0};
    union
    {
        /* Using union ensures correct alignment on some platforms */
        struct cmsghdr cm;
        char buf[CMSG_SPACE(sizeof(pipefd[0]))];
    } cm_un;
    struct msghdr msg = {0};
    struct cmsghdr *cmsg = NULL;
    int bytes_sent = 0;

    endpoint_addr = (struct sockaddr_un *)(&addr->sa);

    if (stat(endpoint_addr->sun_path, &endpoint_stat))
    {
        serr = errno;
	goto error;
    }

    ep_uid = endpoint_stat.st_uid;
    if (ep_uid == 0 || ep_uid == getuid())
    {
        if (pipe(pipefd) != 0)
        {
                serr = errno;
                goto error;
        }
    }

    iovec.iov_base     = &empty_buf;
    iovec.iov_len      = sizeof(empty_buf);

    msg.msg_iov        = &iovec;
    msg.msg_iovlen     = 1;
    msg.msg_control    = cm_un.buf;
    msg.msg_controllen = sizeof(cm_un.buf);
    msg.msg_flags      = 0;

    memset(&cm_un, 0, sizeof(cm_un));

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(pipefd[0]));

    memcpy(CMSG_DATA(cmsg), &pipefd[0], sizeof(pipefd[0]));

    RPC_SOCKET_DISABLE_CANCEL;
    bytes_sent = sendmsg(lrpc->fd, &msg, 0);
    RPC_SOCKET_RESTORE_CANCEL;
    if (bytes_sent == -1)
    {
        serr = errno;
        goto error;
    }

cleanup:

    if (pipefd[0] != -1)
    {
        close(pipefd[0]);
    }

    if (pipefd[1] != -1)
    {
        close(pipefd[1]);
    }

    return serr;

error:

    goto cleanup;
}

INTERNAL rpc_socket_error_t rpc__bsd_socket_recvpeereid
(
    rpc_socket_t        sock,
    uid_t		*euid,
    gid_t		*egid
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;
    int fd = -1;
    int bytes_rcvd = 0;
    struct stat pipe_stat = {0};
    char empty_buf[] = {'\0'};
    rpc_socket_iovec_t iovec = {0};
    union
    {
        /* Using union ensures correct alignment on some platforms */
        struct cmsghdr cm;
        char buf[CMSG_SPACE(sizeof(fd))];
    } cm_un;
    struct cmsghdr *cmsg = NULL;
    struct msghdr msg = {0};

    iovec.iov_base = &empty_buf;
    iovec.iov_len  = sizeof(empty_buf);

    memset(&cm_un, 0, sizeof(cm_un));

    msg.msg_iov        = &iovec;
    msg.msg_iovlen     = 1;
    msg.msg_control    = cm_un.buf;
    msg.msg_controllen = sizeof(cm_un.buf);
    msg.msg_flags      = 0;

    RPC_SOCKET_DISABLE_CANCEL;
    bytes_rcvd = recvmsg(lrpc->fd, &msg, 0);
    RPC_SOCKET_RESTORE_CANCEL;
    if (bytes_rcvd == -1)
    {
        serr = errno;
        goto error;
    }

    if (msg.msg_controllen == 0 ||
        msg.msg_controllen > sizeof(cm_un))
    {
        serr = RPC_C_SOCKET_EACCESS;
        goto error;
    }

    cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg ||
	!(cmsg->cmsg_type == SCM_RIGHTS) ||
        cmsg->cmsg_len - CMSG_ALIGN(sizeof(*cmsg)) != sizeof(fd))
    {
        serr = RPC_C_SOCKET_EACCESS;
        goto error;
    }

    memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));

    if (fstat(fd, &pipe_stat))
    {
        serr = errno;
        goto error;
    }

    if (!S_ISFIFO(pipe_stat.st_mode) ||
        (pipe_stat.st_mode & (S_IRWXO | S_IRWXG)) != 0)
    {
        serr = RPC_C_SOCKET_EACCESS;
        goto error;
    }

    *euid = pipe_stat.st_uid;
    *egid = pipe_stat.st_gid;

cleanup:

    if (fd > 0)
    {
        close(fd);
    }

    return serr;

error:

    *euid = -1;
    *egid = -1;

    goto cleanup;
}

#endif

INTERNAL
int rpc__bsd_socket_get_select_desc(
    rpc_socket_t sock
    )
{
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    return lrpc->fd;
}

INTERNAL
rpc_socket_error_t
rpc__bsd_socket_enum_ifaces(
    rpc_socket_t sock,
    rpc_socket_enum_iface_fn_p_t efun,
    rpc_addr_vector_p_t *rpc_addr_vec,
    rpc_addr_vector_p_t *netmask_addr_vec,
    rpc_addr_vector_p_t *broadcast_addr_vec
)
{
    rpc_ip_addr_p_t         ip_addr = NULL;
    rpc_ip_addr_p_t         netmask_addr = NULL;
    rpc_ip_addr_p_t         broadcast_addr = NULL;
    int                     n_ifs;
    union
    {
        unsigned char           buf[1024];
        struct ifreq req;
    } reqbuf;
    struct ifconf           ifc;
    struct ifreq            *ifr, *last_ifr;
    struct ifreq            ifreq;
    short                   if_flags;
    struct sockaddr         if_addr;
    unsigned int            i;
#ifdef _SOCKADDR_LEN
    int                     prev_size;
#else
    const int prev_size = sizeof(struct ifreq) ;
#endif
    rpc_socket_error_t err = 0;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;

    /*
     * Get the list of network interfaces.
     */
    ifc.ifc_len = sizeof (reqbuf.buf);
    ifc.ifc_buf = (caddr_t) reqbuf.buf;

ifconf_again:
    if (ioctl (lrpc->fd, SIOCGIFCONF, (caddr_t) &ifc) < 0)
    {
        if (errno == EINTR)
        {
            goto ifconf_again;
        }
        err = errno;   /* !!! */
        goto done;
    }

    /*
     * Figure out how many interfaces there must be and allocate an
     * RPC address vector with the appropriate number of elements.
     * (We may ask for a few too many in case some of the interfaces
     * are uninteresting.)
     */
    n_ifs = ifc.ifc_len / sizeof (struct ifreq);
    RPC_DBG_PRINTF(rpc_e_dbg_general, 10,
        ("%lu bytes of ifreqs, ifreq is %lu bytes\n",
         (unsigned long)ifc.ifc_len,
         (unsigned long)sizeof(struct ifreq)));

#ifdef MAX_DEBUG
    if (RPC_DBG2(rpc_e_dbg_general, 15))
    {
        int i;
	char msgbuf[128];

        for (i=0; i<ifc.ifc_len; i++) {
            if ((i % 32) == 0) {
		if (i != 0)
		    RPC_DBG_PRINTF(rpc_e_dbg_general, 15, ("%s\n",msgbuf));
                sprintf(msgbuf, "%4x: ", i);
	    }
            sprintf(msgbuf, "%s%02x ", msgbuf, reqbuf.buf[i]);
        }
	if (i != 0)
	    RPC_DBG_PRINTF(rpc_e_dbg_general, 15, ("%s\n",msgbuf));
    }
#endif

    if (rpc_addr_vec != NULL)
    {
        RPC_MEM_ALLOC (
            *rpc_addr_vec,
            rpc_addr_vector_p_t,
            (sizeof **rpc_addr_vec) + ((n_ifs - 1) * (sizeof (rpc_addr_p_t))),
            RPC_C_MEM_RPC_ADDR_VEC,
            RPC_C_MEM_WAITOK);

        if (*rpc_addr_vec == NULL)
        {
            err = ENOMEM;
            goto done;
        }
    }

    if (netmask_addr_vec != NULL)
    {
        RPC_MEM_ALLOC (
            *netmask_addr_vec,
            rpc_addr_vector_p_t,
         (sizeof **netmask_addr_vec) + ((n_ifs - 1) * (sizeof (rpc_addr_p_t))),
            RPC_C_MEM_RPC_ADDR_VEC,
            RPC_C_MEM_WAITOK);

        if (*netmask_addr_vec == NULL)
        {
            err = ENOMEM;
            RPC_MEM_FREE (*netmask_addr_vec, RPC_C_MEM_RPC_ADDR_VEC);
            goto done;
        }

        (*netmask_addr_vec)->len = 0;
    }

    if (broadcast_addr_vec != NULL)
    {
        RPC_MEM_ALLOC (
            *broadcast_addr_vec,
            rpc_addr_vector_p_t,
         (sizeof **broadcast_addr_vec) + ((n_ifs - 1) * (sizeof (rpc_addr_p_t))),
            RPC_C_MEM_RPC_ADDR_VEC,
            RPC_C_MEM_WAITOK);

        if (*broadcast_addr_vec == NULL)
        {
            err = ENOMEM;
            RPC_MEM_FREE (*broadcast_addr_vec, RPC_C_MEM_RPC_ADDR_VEC);
            goto done;
        }

        (*broadcast_addr_vec)->len = 0;
    }

    /*
     * Go through the interfaces and get the info associated with them.
     */
    assert(rpc_addr_vec != NULL);
    (*rpc_addr_vec)->len = 0;
    last_ifr = (struct ifreq *)(void *) (ifc.ifc_buf + ifc.ifc_len);

    for (i=0, ifr = ifc.ifc_req; ifr < last_ifr ;
         i++, ifr = (struct ifreq *)(void *)(( (char *) ifr ) + prev_size))
    {
#ifdef _SOCKADDR_LEN
        prev_size = sizeof (struct ifreq) - sizeof(struct sockaddr) + ifr->ifr_addr.sa_len ;
#endif
        RPC_DBG_PRINTF(rpc_e_dbg_general, 10, ("interface %d: %s\n",
            i, ifr->ifr_name));
        /*
         * Get the interface's flags.  If the flags say that the interface
         * is not up or is the loopback interface, skip it.  Do the
         * SIOCGIFFLAGS on a copy of the ifr so we don't lose the original
         * contents of the ifr.  (ifr's are unions that hold only one
         * of the interesting interface attributes [address, flags, etc.]
         * at a time.)
         */
        memcpy(&ifreq, ifr, sizeof(ifreq));
ifflags_again:
        if (ioctl(lrpc->fd, SIOCGIFFLAGS, &ifreq) < 0)
        {
            RPC_DBG_PRINTF(rpc_e_dbg_general, 10,
                ("SIOCGIFFLAGS returned errno %d\n", errno));
            if (errno == EINTR)
            {
                goto ifflags_again;
            }
            continue;
        }
        if_flags = ifreq.ifr_flags;     /* Copy out the flags */
        RPC_DBG_PRINTF(rpc_e_dbg_general, 10, ("flags are %x\n", if_flags));

        /*
         * Ignore interfaces which are not 'up'.
         */
        if ((if_flags & IFF_UP) == 0)

	  continue;

#ifndef USE_LOOPBACK
	/*
	 * Ignore the loopback interface
	 */

        if (if_flags & IFF_LOOPBACK) continue;
#endif
	/*
	 * Ignore Point-to-Point interfaces (i.e. SLIP/PPP )
         * *** NOTE:  We need an Environment Variable Evaluation at
	 * some point so we can selectively allow RPC servers to
	 * some up with/without SLIP/PPP bindings. For Dynamic PPP/SLIP
	 * interfaces, this creates problems for now.
	 */

        if (if_flags & IFF_POINTOPOINT) continue;

        /*
         * Get the addressing stuff for this interface.
         */

#ifdef NO_SIOCGIFADDR

        /*
         * Note that some systems do not return the address for the
         * interface given.  However the ifr array elts contained in
         * the ifc block returned from the SIOCGIFCONF ioctl above already
         * contains the correct addresses. So these systems should define
         * NO_SIOCGIFADDR in their platform specific include file.
         */
        if_addr = ifr->ifr_addr;

#else

        /*
         * Do the SIOCGIFADDR on a copy of the ifr.  See above.
         */
        memcpy(&ifreq, ifr, sizeof(ifreq));
    ifaddr_again:
        if (ioctl(lrpc->fd, SIOCGIFADDR, &ifreq) < 0)
        {
            /*
             * UP but no ip address, skip it
             */
            if (errno == EADDRNOTAVAIL) continue;

            RPC_DBG_PRINTF(rpc_e_dbg_general, 10,
                           ("SIOCGIFADDR returned errno %d\n", errno));
            if (errno == EINTR)
            {
                goto ifaddr_again;
            }

            err = errno;
            goto FREE_IT;
        }

        memcpy (&if_addr, &ifr->ifr_addr, sizeof(struct sockaddr));

#endif  /* NO_SIOCGIFADDR */

        /*
         * If this isn't an Internet-family address, ignore it.
         */
        if (if_addr.sa_family != AF_INET)
        {
            RPC_DBG_PRINTF(rpc_e_dbg_general, 10, ("AF %d not INET\n",
                                                   if_addr.sa_family));
            continue;
        }

        if (rpc_addr_vec != NULL)
        {
            /*
             * Allocate and fill in an IP RPC address for this interface.
             */
            RPC_MEM_ALLOC (
                ip_addr,
                rpc_ip_addr_p_t,
                sizeof (rpc_ip_addr_t),
                RPC_C_MEM_RPC_ADDR,
                RPC_C_MEM_WAITOK);

            if (ip_addr == NULL)
            {
                err = ENOMEM;
                goto FREE_IT;
            }

            ip_addr->rpc_protseq_id = sock->pseq_id;
            ip_addr->len            = sizeof (struct sockaddr_in);

            memcpy (&ip_addr->sa, &if_addr, sizeof(struct sockaddr_in));
        }
        else
        {
            ip_addr = NULL;
        }

        if (netmask_addr_vec != NULL && (if_flags & IFF_LOOPBACK) == 0)
        {
            memcpy(&ifreq, ifr, sizeof(ifreq));

            while (ioctl(lrpc->fd, SIOCGIFNETMASK, &ifreq) == -1)
            {
                if (errno != EINTR)
                {
                    err = errno;
                    goto FREE_IT;
                }
            }

            RPC_MEM_ALLOC (
                netmask_addr,
                rpc_ip_addr_p_t,
                sizeof (rpc_ip_addr_t),
                RPC_C_MEM_RPC_ADDR,
                RPC_C_MEM_WAITOK);

            if (netmask_addr == NULL)
            {
                err = ENOMEM;
                goto FREE_IT;
            }

            netmask_addr->rpc_protseq_id = sock->pseq_id;
            netmask_addr->len            = sizeof (struct sockaddr_in);
            memcpy(&netmask_addr->sa, &ifreq.ifr_addr, sizeof(struct sockaddr_in));
        }
        else
        {
            netmask_addr = NULL;
        }

        if (broadcast_addr_vec != NULL && (if_flags & IFF_BROADCAST))
        {
            memcpy(&ifreq, ifr, sizeof(ifreq));

            while (ioctl(lrpc->fd, SIOCGIFBRDADDR, &ifreq) == -1)
            {
                if (errno != EINTR)
                {
                    err = errno;
                    goto FREE_IT;
                }
            }

            RPC_MEM_ALLOC (
                broadcast_addr,
                rpc_ip_addr_p_t,
                sizeof (rpc_ip_addr_t),
                RPC_C_MEM_RPC_ADDR,
                RPC_C_MEM_WAITOK);

            if (broadcast_addr == NULL)
            {
                err = ENOMEM;
                goto FREE_IT;
            }

            broadcast_addr->rpc_protseq_id = sock->pseq_id;
            broadcast_addr->len            = sizeof (struct sockaddr_in);
            memcpy(&broadcast_addr->sa, &ifreq.ifr_broadaddr, sizeof(struct sockaddr_in));
        }
        else
        {
            broadcast_addr = NULL;
        }

        /*
         * Call out to do any final filtering and get the desired IP address
         * for this interface.  If the callout function returns false, we
         * forget about this interface.
         */
        if ((*efun) (sock, (rpc_addr_p_t) ip_addr, (rpc_addr_p_t) netmask_addr, (rpc_addr_p_t) broadcast_addr) == false)
        {
            if (ip_addr != NULL)
            {
                RPC_MEM_FREE (ip_addr, RPC_C_MEM_RPC_ADDR);
                ip_addr = NULL;
            }
            if (netmask_addr != NULL)
            {
                RPC_MEM_FREE (netmask_addr, RPC_C_MEM_RPC_ADDR);
                netmask_addr = NULL;
            }
            if (broadcast_addr != NULL)
            {
                RPC_MEM_FREE (broadcast_addr, RPC_C_MEM_RPC_ADDR);
                broadcast_addr = NULL;
            }
            continue;
        }

        if (rpc_addr_vec != NULL && ip_addr != NULL)
        {
            (*rpc_addr_vec)->addrs[(*rpc_addr_vec)->len++]
                = (rpc_addr_p_t) ip_addr;
            ip_addr = NULL;
        }
        if (netmask_addr_vec != NULL && netmask_addr != NULL)
        {
            (*netmask_addr_vec)->addrs[(*netmask_addr_vec)->len++]
                = (rpc_addr_p_t) netmask_addr;
            netmask_addr = NULL;
        }
        if (broadcast_addr_vec != NULL && broadcast_addr != NULL)
        {
            (*broadcast_addr_vec)->addrs[(*broadcast_addr_vec)->len++]
                = (rpc_addr_p_t) broadcast_addr;
            broadcast_addr = NULL;
        }
    }

    if ((*rpc_addr_vec)->len == 0)
    {
        err = EINVAL;   /* !!! */
        goto FREE_IT;
    }

    err = RPC_C_SOCKET_OK;
done:

    return err;

FREE_IT:

    if (ip_addr != NULL)
    {
        RPC_MEM_FREE (ip_addr, RPC_C_MEM_RPC_ADDR);
    }
    if (netmask_addr != NULL)
    {
        RPC_MEM_FREE (netmask_addr, RPC_C_MEM_RPC_ADDR);
    }
    if (broadcast_addr != NULL)
    {
        RPC_MEM_FREE (broadcast_addr, RPC_C_MEM_RPC_ADDR);
    }

    if (rpc_addr_vec != NULL && *rpc_addr_vec != NULL)
    {
        for (i = 0; i < (*rpc_addr_vec)->len; i++)
        {
            RPC_MEM_FREE ((*rpc_addr_vec)->addrs[i], RPC_C_MEM_RPC_ADDR);
        }
        RPC_MEM_FREE (*rpc_addr_vec, RPC_C_MEM_RPC_ADDR_VEC);
        *rpc_addr_vec = NULL;
    }
    if (netmask_addr_vec != NULL && *netmask_addr_vec != NULL)
    {
        for (i = 0; i < (*netmask_addr_vec)->len; i++)
        {
            RPC_MEM_FREE ((*netmask_addr_vec)->addrs[i], RPC_C_MEM_RPC_ADDR);
        }
        RPC_MEM_FREE (*netmask_addr_vec, RPC_C_MEM_RPC_ADDR_VEC);
        *netmask_addr_vec = NULL;
    }
    if (broadcast_addr_vec != NULL && *broadcast_addr_vec != NULL)
    {
        assert(broadcast_addr_vec != NULL);
        for (i = 0; i < (*broadcast_addr_vec)->len; i++)
        {
            RPC_MEM_FREE ((*broadcast_addr_vec)->addrs[i], RPC_C_MEM_RPC_ADDR);
        }
        RPC_MEM_FREE (*broadcast_addr_vec, RPC_C_MEM_RPC_ADDR_VEC);
        *broadcast_addr_vec = NULL;
    }

    goto done;
}

INTERNAL
rpc_socket_error_t
rpc__bsd_socket_inq_transport_info(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    rpc_transport_info_handle_t* info
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_bsd_socket_p_t lrpc = (rpc_bsd_socket_p_t) sock->data.pointer;
    rpc_bsd_transport_info_p_t lrpc_info = NULL;

    lrpc_info = calloc(1, sizeof(*lrpc_info));

    if (!lrpc_info)
    {
        serr = ENOMEM;
	goto error;
    }

    lrpc_info->peer_uid = lrpc->info.peer_uid;
    lrpc_info->peer_gid = lrpc->info.peer_gid;

    *info = (rpc_transport_info_handle_t) lrpc_info;

error:
    if (serr)
    {
        *info = NULL;

	if (lrpc_info)
        {
            rpc_lrpc_transport_info_free((rpc_transport_info_handle_t) lrpc_info);
        }
    }

    return serr;
}

void
rpc_lrpc_transport_info_free(
    rpc_transport_info_handle_t info
    )
{
    if (info)
    {
        free(info);
    }
}

void
rpc_lrpc_transport_info_inq_peer_eid(
    rpc_transport_info_handle_t info,
    unsigned32                  *uid,
    unsigned32                  *gid
    )
{
    rpc_bsd_transport_info_p_t lrpc_info = (rpc_bsd_transport_info_p_t) info;

    if (uid)
    {
        *uid = lrpc_info->peer_uid;
    }

    if (gid)
    {
        *gid = lrpc_info->peer_gid;
    }
}

INTERNAL
boolean
rpc__bsd_socket_transport_info_equal(
    rpc_transport_info_handle_t info1,
    rpc_transport_info_handle_t info2
    )
{
    rpc_bsd_transport_info_p_t bsd_info1 = (rpc_bsd_transport_info_p_t) info1;
    rpc_bsd_transport_info_p_t bsd_info2 = (rpc_bsd_transport_info_p_t) info2;

    if ((bsd_info1 != NULL) && (bsd_info2 != NULL))
    {
        return
            (bsd_info1->peer_uid == bsd_info2->peer_uid &&
             bsd_info1->peer_gid == bsd_info2->peer_gid);
    } else if (bsd_info1 == bsd_info2){
        return true;
    }

    rpc_bsd_transport_info_p_t tmp = bsd_info1 != NULL ? bsd_info1: bsd_info2;
    return  (tmp->peer_uid == getuid() && tmp->peer_gid == getgid());
}

INTERNAL
rpc_socket_error_t
rpc__bsd_socket_transport_inq_access_token(
    rpc_transport_info_handle_t info ATTRIBUTE_UNUSED,
    rpc_access_token_p_t* token ATTRIBUTE_UNUSED
    )
{
#if HAVE_LIKEWISE_LWMAPSECURITY
    rpc_bsd_transport_info_p_t lrpc_info = (rpc_bsd_transport_info_p_t) info;
    NTSTATUS status = STATUS_SUCCESS;
    PLW_MAP_SECURITY_CONTEXT context = NULL;

    status = LwMapSecurityCreateContext(&context);
    if (status) goto error;

    status = LwMapSecurityCreateAccessTokenFromUidGid(
        context,
        token,
        lrpc_info->peer_uid,
        lrpc_info->peer_gid);
    if (status) goto error;

error:

    LwMapSecurityFreeContext(&context);

    return LwNtStatusToErrno(status);
#else
    return RPC_C_SOCKET_ENOTSUP;
#endif
}

PRIVATE const rpc_socket_vtbl_t rpc_g_bsd_socket_vtbl =
{
    .socket_duplicate = rpc__bsd_socket_duplicate,
    .socket_construct = rpc__bsd_socket_construct,
    .socket_destruct = rpc__bsd_socket_destruct,
    .socket_bind = rpc__bsd_socket_bind,
    .socket_connect = rpc__bsd_socket_connect,
    .socket_accept = rpc__bsd_socket_accept,
    .socket_listen = rpc__bsd_socket_listen,
    .socket_sendmsg = rpc__bsd_socket_sendmsg,
    .socket_recvfrom = rpc__bsd_socket_recvfrom,
    .socket_recvmsg = rpc__bsd_socket_recvmsg,
    .socket_inq_endpoint = rpc__bsd_socket_inq_endpoint,
    .socket_set_broadcast = rpc__bsd_socket_set_broadcast,
    .socket_set_bufs = rpc__bsd_socket_set_bufs,
    .socket_set_nbio = rpc__bsd_socket_set_nbio,
    .socket_set_close_on_exec = rpc__bsd_socket_set_close_on_exec,
    .socket_getpeername = rpc__bsd_socket_getpeername,
    .socket_get_if_id = rpc__bsd_socket_get_if_id,
    .socket_set_keepalive = rpc__bsd_socket_set_keepalive,
    .socket_nowriteblock_wait = rpc__bsd_socket_nowriteblock_wait,
    .socket_set_rcvtimeo = rpc__bsd_socket_set_rcvtimeo,
    .socket_getpeereid = rpc__bsd_socket_getpeereid,
    .socket_get_select_desc = rpc__bsd_socket_get_select_desc,
    .socket_enum_ifaces = rpc__bsd_socket_enum_ifaces,
    .socket_inq_transport_info = rpc__bsd_socket_inq_transport_info,
    .transport_info_free = rpc_lrpc_transport_info_free,
    .transport_info_equal = rpc__bsd_socket_transport_info_equal,
    .transport_inq_access_token = rpc__bsd_socket_transport_inq_access_token
};
