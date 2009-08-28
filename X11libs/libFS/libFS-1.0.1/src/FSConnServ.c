/* $Xorg: FSConnServ.c,v 1.4 2001/02/09 02:03:25 xorgcvs Exp $ */

/*
 * Copyright 1990 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software 
 * and its documentation for any purpose is hereby granted without fee, 
 * provided that the above copyright notice appear in all copies and 
 * that both that copyright notice and this permission notice appear 
 * in supporting documentation, and that the names of Network Computing 
 * Devices or Digital not be used in advertising or publicity pertaining 
 * to distribution of the software without specific, written prior 
 * permission. Network Computing Devices or Digital make no representations 
 * about the suitability of this software for any purpose.  It is provided 
 * "as is" without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES AND  DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES 
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, 
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS 
 * SOFTWARE.
 */

/*

Copyright 1987, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
/* $XFree86: xc/lib/FS/FSConnServ.c,v 3.10 2001/10/28 03:32:27 tsi Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include	"FSlibint.h"
#include	<stdio.h>
#include	"X11/Xpoll.h"
#ifdef NCD
#include	<fcntl.h>
#endif
#ifdef WIN32
#define ECHECK(err) (WSAGetLastError() == err)
#else
#ifdef ISC
#define ECHECK(err) ((errno == err) || errno == EAGAIN || errno == EWOULDBLOCK)
#else
#define ECHECK(err) (errno == err)
#endif
#endif

/*
 * Attempts to connect to server, given server name. Returns transport
 * connection object or NULL if connection fails.
 */

#define FS_CONNECTION_RETRIES 5

XtransConnInfo
_FSConnectServer(char *server_name)
{
    XtransConnInfo trans_conn = NULL;	/* transport connection object */
    int retry, connect_stat;
    int  madeConnection = 0;

    /*
     * Open the network connection.
     */

    for (retry = FS_CONNECTION_RETRIES; retry >= 0; retry--)
    {
	if ((trans_conn = _FSTransOpenCOTSClient(server_name)) == NULL)
	{
	    break;
	}

	if ((connect_stat = _FSTransConnect(trans_conn,server_name)) < 0)
	{
	    _FSTransClose(trans_conn);

	    if (connect_stat == TRANS_TRY_CONNECT_AGAIN)
	    {
		sleep(1);
		continue;
	    }
	    else
		break;
	}
	else
	{
	    madeConnection = 1;
	    break;
	}
    }

    if (!madeConnection)
	return (NULL);


    /*
     * set it non-blocking.  This is so we can read data when blocked for
     * writing in the library.
     */

    _FSTransSetOption(trans_conn, TRANS_NONBLOCKING, 1);

    return (trans_conn);
}

/*
 * Disconnect from server.
 */

void
_FSDisconnectServer(XtransConnInfo trans_conn)

{
    (void) _FSTransClose(trans_conn);
}


/*
 * This is an OS dependent routine which:
 * 1) returns as soon as the connection can be written on....
 * 2) if the connection can be read, must enqueue events and handle errors,
 * until the connection is writable.
 */
void _FSWaitForWritable(FSServer *svr)
{
    fd_set	r_mask;
    fd_set	w_mask;
    int         nfound;

    FD_ZERO(&r_mask);
    FD_ZERO(&w_mask);

    while (1) {
	FD_SET(svr->fd, &r_mask);
	FD_SET(svr->fd, &w_mask);

	do {
	    nfound = Select(svr->fd + 1, &r_mask, &w_mask, NULL, NULL);
	    if (nfound < 0 && !ECHECK(EINTR))
		(*_FSIOErrorFunction) (svr);
	} while (nfound <= 0);

	if (XFD_ANYSET(&r_mask)) {
	    char        buf[BUFSIZE];
	    BytesReadable_t pend_not_register;
	    register BytesReadable_t pend;
	    register fsEvent *ev;

	    /* find out how much data can be read */
	    if (_FSTransBytesReadable(svr->trans_conn, &pend_not_register) < 0)
		(*_FSIOErrorFunction) (svr);
	    pend = pend_not_register;

	    /*
	     * must read at least one fsEvent; if none is pending, then we'll
	     * just block waiting for it
	     */
	    if (pend < SIZEOF(fsEvent))
		pend = SIZEOF(fsEvent);

	    /* but we won't read more than the max buffer size */
	    if (pend > BUFSIZE)
		pend = BUFSIZE;

	    /* round down to an integral number of FSReps */
	    pend = (pend / SIZEOF(fsEvent)) * SIZEOF(fsEvent);

	    _FSRead(svr, buf, pend);

	    /* no space between comma and type or else macro will die */
	    STARTITERATE(ev, fsEvent, buf, (pend > 0),
			 (pend -= SIZEOF(fsEvent))) {
		if (ev->type == FS_Error)
		    _FSError(svr, (fsError *) ev);
		else		/* it's an event packet; enqueue it */
		    _FSEnq(svr, ev);
	    }
	    ENDITERATE
	}
	if (XFD_ANYSET(&w_mask))
	    return;
    }
}


void _FSWaitForReadable(FSServer *svr)
{
    fd_set	r_mask;
    int         result;

    FD_ZERO(&r_mask);
    do {
	FD_SET(svr->fd, &r_mask);
	result = Select(svr->fd + 1, &r_mask, NULL, NULL, NULL);
	if (result == -1 && !ECHECK(EINTR))
	    (*_FSIOErrorFunction) (svr);
    } while (result <= 0);
}

void _FSSendClientPrefix(
    FSServer		*svr,
    fsConnClientPrefix	*client)
{
    struct iovec iovarray[5],
               *iov = iovarray;
    int         niov = 0;

#define add_to_iov(b,l) \
	  { iov->iov_base = (b); iov->iov_len = (l); iov++, niov++; }

    add_to_iov((caddr_t) client, SIZEOF(fsConnClientPrefix));

#undef add_to_iov

    (void) _FSTransWritev(svr->trans_conn, iovarray, niov);
    return;
}
