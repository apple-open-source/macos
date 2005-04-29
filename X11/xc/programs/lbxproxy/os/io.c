/* $Xorg: io.c,v 1.6 2001/02/09 02:05:33 xorgcvs Exp $ */
/***********************************************************

Copyright 1987, 1989, 1998  The Open Group

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


Copyright 1987, 1989 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* $XFree86: xc/programs/lbxproxy/os/io.c,v 1.11 2003/11/17 22:20:49 dawes Exp $ */
/*****************************************************************
 * i/o functions
 *
 *   WriteToClient, ReadRequestFromClient
 *   InsertFakeRequest, ResetCurrentRequest
 *
 *****************************************************************/

#include <stdio.h>
#include <X11/Xtrans.h>
#include "Xos.h"
#include "misc.h"
#include <errno.h>
#include <sys/param.h>
#include <sys/uio.h>
#include "os.h"
#include <X11/Xpoll.h>
#include "osdep.h"
#include "lbx.h"

#ifdef BIGREQS
#include "bigreqstr.h"
#endif

/* check for both EAGAIN and EWOULDBLOCK, because some supposedly POSIX
 * systems are broken and return EWOULDBLOCK when they should return EAGAIN
 */
#if defined(EAGAIN) && defined(EWOULDBLOCK)
#define ETEST(err) (err == EAGAIN || err == EWOULDBLOCK)
#else
#ifdef EAGAIN
#define ETEST(err) (err == EAGAIN)
#else
#define ETEST(err) (err == EWOULDBLOCK)
#endif
#endif

extern void MarkClientException();
static int timesThisConnection = 0;
static ConnectionInputPtr FreeInputs = (ConnectionInputPtr)NULL;
static ConnectionOutputPtr FreeOutputs = (ConnectionOutputPtr)NULL;
static OsCommPtr AvailableInput = (OsCommPtr)NULL;

static ConnectionInputPtr AllocateInputBuffer(
    void
);
static ConnectionOutputPtr AllocateOutputBuffer(
    void
);
static ConnectionOutputPtr AllocateUncompBuffer(
    int count
);

#define get_req_len(req,cli) (((cli)->swapped ? \
			      lswaps((req)->length) : (req)->length) << 2)


unsigned long
StandardRequestLength(req,client,got,partp)
    xReq	*req;
    ClientPtr	client;
    int		got;
    Bool    	*partp;
{
    int	    len;
    
    if (!req)
	req = (xReq *) client->requestBuffer;
    if (got < sizeof (xReq))
    {
	*partp = TRUE;
	return sizeof (xReq);
    }
    len = get_req_len(req,client);
    if (len > MAXBUFSIZE)
    {
	*partp = TRUE;
	return -1;
    }
    *partp = FALSE;
    return len;
}

#ifdef BIGREQS
#define get_big_req_len(req,cli) (((cli)->swapped ? \
				  lswapl(((xBigReq *)(req))->length) : \
				  ((xBigReq *)(req))->length) << 2)

#endif

#define MAX_TIMES_PER         10


/*
 *   A lot of the code in this file manipulates a ConnectionInputPtr:
 *
 *    -----------------------------------------------
 *   |------- bufcnt ------->|           |           |
 *   |           |- gotnow ->|           |           |
 *   |           |-------- needed ------>|           |
 *   |-----------+--------- size --------+---------->|
 *    -----------------------------------------------
 *   ^           ^
 *   |           |
 *   buffer   bufptr
 *
 *  buffer is a pointer to the start of the buffer.
 *  bufptr points to the start of the current request.
 *  bufcnt counts how many bytes are in the buffer.
 *  size is the size of the buffer in bytes.
 *
 *  In several of the functions, gotnow and needed are local variables
 *  that do the following:
 *
 *  gotnow is the number of bytes of the request that we're
 *  trying to read that are currently in the buffer.
 *  Typically, gotnow = (buffer + bufcnt) - bufptr
 *
 *  needed = the length of the request that we're trying to
 *  read in bytes.
 */


/*****************************************************************
 * ReadRequestFromClient
 *    Returns one request in client->requestBuffer.  The request
 *    length will be in client->req_len.  Return status is:
 *
 *    > 0  if  successful, specifies length in bytes of the request
 *    = 0  if  entire request is not yet available
 *    < 0  if  client should be terminated
 *
 *    The request returned must be contiguous so that it can be
 *    cast in the dispatcher to the correct request type.  Because requests
 *    are variable length, ReadRequestFromClient() must look at the first 4
 *    or 8 bytes of a request to determine the length (the request length is
 *    in the 3rd and 4th bytes of the request unless it is a Big Request
 *    (see the Big Request Extension), in which case the 3rd and 4th bytes
 *    are zero and the following 4 bytes are the request length.
 *
 *    Note: in order to make the server scheduler (WaitForSomething())
 *    "fair", the ClientsWithInput mask is used.  This mask tells which
 *    clients have FULL requests left in their buffers.  Clients with
 *    partial requests require a read.  Basically, client buffers
 *    are drained before select() is called again.  But, we can't keep
 *    reading from a client that is sending buckets of data (or has
 *    a partial request) because others clients need to be scheduled.
 *****************************************************************/

#define YieldControl()				\
        { isItTimeToYield = TRUE;		\
	  timesThisConnection = 0; }
#define YieldControlNoInput()			\
        { YieldControl();			\
	  FD_CLR(fd, &ClientsWithInput); }
#define YieldControlDeath()			\
        { timesThisConnection = 0; }

int
StandardReadRequestFromClient(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;
    int fd = oc->fd;
    register int gotnow, needed;
    int result;
    register xReq *request;
    int	nextneeds;
    Bool part;
#ifdef BIGREQS
    Bool move_header;
#endif

    /* If an input buffer was empty, either free it if it is too big
     * or link it into our list of free input buffers.  This means that
     * different clients can share the same input buffer (at different
     * times).  This was done to save memory.
     */

    if (AvailableInput)
    {
	if (AvailableInput != oc)
	{
	    register ConnectionInputPtr aci = AvailableInput->input;
	    if (aci->size > BUFWATERMARK)
	    {
		xfree(aci->buffer);
		xfree(aci);
	    }
	    else
	    {
		aci->next = FreeInputs;
		FreeInputs = aci;
	    }
	    AvailableInput->input = (ConnectionInputPtr)NULL;
	}
	AvailableInput = (OsCommPtr)NULL;
    }

    /* make sure we have an input buffer */

    if (!oci)
    {
	if ((oci = FreeInputs))
	{
	    FreeInputs = oci->next;
	}
	else if (!(oci = AllocateInputBuffer()))
	{
	    YieldControlDeath();
	    return -1;
	}
	oc->input = oci;
    }

    /* advance to start of next request */

    oci->bufptr += oci->lenLastReq;
    oci->lenLastReq = 0;

#ifdef BIGREQS
    move_header = FALSE;
#endif
    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;
    client->requestBuffer = (pointer)oci->bufptr;
    needed = RequestLength (NULL, client, gotnow, &part);

#ifdef BIGREQS
    if (!needed && client->big_requests)
    {
	/* It's a Big Request. */
	move_header = TRUE;
	if (gotnow < sizeof(xBigReq))
	{
	    /* Still need more data to tell just how big. */
	    needed = sizeof(xBigReq);
	    part = TRUE;
	}
	else
	    needed = get_big_req_len(client->requestBuffer, client);
    }
#endif

    client->req_len = needed >> 2;
    if (gotnow < needed || part)
    {
	/* Need to read more data, either so that we can get a
	 * complete xReq (if part is TRUE), a complete
	 * xBigReq (if move_header is TRUE), or the rest of the
	 * request (if part and move_header are both FALSE).
	 */

	if (needed == -1)
	{
	    YieldControlDeath();
	    return -1;
	}
	if ((gotnow == 0) ||
	    ((oci->bufptr - oci->buffer + needed) > oci->size))
	{
	    /* no data, or the request is too big to fit in the buffer */

	    if ((gotnow > 0) && (oci->bufptr != oci->buffer))
		/* save the data we've already read */
		memmove(oci->buffer, oci->bufptr, gotnow);
	    if (needed > oci->size)
	    {
		/* make buffer bigger to accomodate request */
		char *ibuf;

		ibuf = (char *)xrealloc(oci->buffer, needed);
		if (!ibuf)
		{
		    YieldControlDeath();
		    return -1;
		}
		oci->size = needed;
		oci->buffer = ibuf;
	    }
	    oci->bufptr = oci->buffer;
	    oci->bufcnt = gotnow;
	}

	if (oc->trans_conn)
	    result = _LBXPROXYTransRead(oc->trans_conn, 
					oci->buffer + oci->bufcnt,
				        oci->size - oci->bufcnt);
	else
	    /*
	     * trans_conn can be NULL if the read is from an X server
	     */
	    result = (*oc->Read)(fd, oci->buffer + oci->bufcnt,
				  oci->size - oci->bufcnt);

	if (result <= 0)
	{
	    if ((result < 0) && ETEST(errno))
	    {
		YieldControlNoInput();
		return 0;
	    }
	    YieldControlDeath();
	    return -1;
	}
	oci->bufcnt += result;
	gotnow += result;
	/* free up some space after huge requests */
	if ((oci->size > BUFWATERMARK) &&
	    (oci->bufcnt < BUFSIZE) && (needed < BUFSIZE))
	{
	    char *ibuf;

	    ibuf = (char *)xrealloc(oci->buffer, BUFSIZE);
	    if (ibuf)
	    {
		oci->size = BUFSIZE;
		oci->buffer = ibuf;
		oci->bufptr = ibuf + oci->bufcnt - gotnow;
	    }
	}
	client->requestBuffer = (pointer) oci->bufptr;
	if (part && gotnow >= needed)
	{
	    /* We wanted an xReq, now we've gotten it. */
	    needed = RequestLength (NULL, client, gotnow, &part);
#ifdef BIGREQS
	    if (!needed && client->big_requests)
	    {
		move_header = TRUE;
		if (gotnow < sizeof(xBigReq))
		    needed = sizeof(xBigReq);
		else
		    needed = get_big_req_len(client->requestBuffer, client);
	    }
#endif
	    client->req_len = needed >> 2;
	}
	if (gotnow < needed || part)
	{
	    if (needed == -1)
	    {
		YieldControlDeath();
		return -1;
	    }
	    /* Still don't have enough; punt. */
	    YieldControlNoInput();
	    return 0;
	}
    }

    if (needed == 0)
    {
#ifdef BIGREQS
	if (client->big_requests)
	    needed = sizeof(xBigReq);
	else
#endif
	    needed = sizeof(xReq);
    }

    oci->lenLastReq = needed;

    /*
     *  Check to see if client has at least one whole request in the
     *  buffer beyond the request we're returning to the caller.
     *  If there is only a partial request, treat like buffer
     *  is empty so that select() will be called again and other clients
     *  can get into the queue.   
     */

    if (gotnow > needed)
    {
	request = (xReq *)(oci->bufptr + needed);
	nextneeds = RequestLength (request, client, gotnow - needed, &part);
	if (gotnow >= needed + nextneeds && !part
#ifdef BIGREQS
	    && (nextneeds ||
		(client->big_requests &&
		 (nextneeds >= sizeof(xBigReq) &&
		  nextneeds >= get_big_req_len(request, client))))
#endif
	    )
	    FD_SET(fd, &ClientsWithInput);
	else
	    YieldControlNoInput();
    }
    else
    {
	    AvailableInput = oc;
	YieldControlNoInput();
    }
    if (++timesThisConnection >= MAX_TIMES_PER)
	YieldControl();
#ifdef BIGREQS
    if (move_header)
    {
	request = (xReq *)oci->bufptr;
	oci->bufptr += (sizeof(xBigReq) - sizeof(xReq));
	*(xReq *)oci->bufptr = *request;
	oci->lenLastReq -= (sizeof(xBigReq) - sizeof(xReq));
	client->req_len -= (sizeof(xBigReq) - sizeof(xReq)) >> 2;
	client->requestBuffer = oci->bufptr;
    }
#endif
    return needed;
}

int
PendingClientOutput (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr) client->osPrivate;
    ConnectionOutputPtr	oco = oc->output;
    
    return (oco && oco->count != 0) || oc->ofirst;
}

int
CheckPendingClientInput (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;
    xReq    *request;
    int	    gotnow;
    int	    needed;
    Bool    part;
    
    if (!oci)
	return 0;
    needed = oci->lenLastReq;
    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;
    request = (xReq *) (oci->bufptr + needed);
    if (gotnow >= needed + RequestLength(request, client, gotnow - needed, &part) && !part)
    {
	FD_SET(oc->fd, &ClientsWithInput);
	return 1;
    }
    return 0;
}

void
MarkConnectionWriteBlocked (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;

    FD_SET(oc->fd, &ClientsWriteBlocked);
    AnyClientsWriteBlocked = TRUE;
}

int
BytesInClientBuffer (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;

    if (!oci)
	return 0;
    return oci->bufcnt + oci->buffer - (oci->bufptr + oci->lenLastReq);
}

void
SkipInClientBuffer (client, nbytes, lenLastReq)
    ClientPtr	client;
    int		nbytes;
    int		lenLastReq;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;

    if (!oci)
	return;
    oci->bufptr += nbytes;
    oci->lenLastReq = lenLastReq;
}

/*****************************************************************
 * InsertFakeRequest
 *    Splice a consed up (possibly partial) request in as the next request.
 *
 **********************/

Bool
InsertFakeRequest(client, data, count)
    ClientPtr client;
    char *data;
    int count;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;
    int fd = oc->fd;
    register xReq *request;
    register int gotnow, moveup;
    Bool part;

    if (AvailableInput)
    {
	if (AvailableInput != oc)
	{
	    register ConnectionInputPtr aci = AvailableInput->input;
	    if (aci->size > BUFWATERMARK)
	    {
		xfree(aci->buffer);
		xfree(aci);
	    }
	    else
	    {
		aci->next = FreeInputs;
		FreeInputs = aci;
	    }
	    AvailableInput->input = (ConnectionInputPtr)NULL;
	}
	AvailableInput = (OsCommPtr)NULL;
    }
    if (!oci)
    {
	if ((oci = FreeInputs))
	    FreeInputs = oci->next;
	else if (!(oci = AllocateInputBuffer()))
	    return FALSE;
	oc->input = oci;
    }
    oci->bufptr += oci->lenLastReq;
    oci->lenLastReq = 0;
    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;
    if ((gotnow + count) > oci->size)
    {
	char *ibuf;

	ibuf = (char *)xrealloc(oci->buffer, gotnow + count);
	if (!ibuf)
	    return(FALSE);
	oci->size = gotnow + count;
	oci->buffer = ibuf;
	oci->bufptr = ibuf + oci->bufcnt - gotnow;
    }
    moveup = count - (oci->bufptr - oci->buffer);
    if (moveup > 0)
    {
	if (gotnow > 0)
	    memmove(oci->bufptr + moveup, oci->bufptr, gotnow);
	oci->bufptr += moveup;
	oci->bufcnt += moveup;
    }
    memmove(oci->bufptr - count, data, count);
    oci->bufptr -= count;
    request = (xReq *)oci->bufptr;
    gotnow += count;
    if (gotnow >= RequestLength (request, client, gotnow, &part) && !part)
	FD_SET(fd, &ClientsWithInput);
    else
	YieldControlNoInput();
    return(TRUE);
}

/*****************************************************************
 * ResetRequestFromClient
 *    Reset to reexecute the current request, and yield.
 *
 **********************/

void
ResetCurrentRequest(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;
    int fd = oc->fd;
    register xReq *request;
    int gotnow;
    Bool part;

    if (AvailableInput == oc)
	AvailableInput = (OsCommPtr)NULL;
    oci->lenLastReq = 0;
    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;
    request = (xReq *)oci->bufptr;
    if (gotnow >= RequestLength (request, client, gotnow, &part) && !part)
    {
	FD_SET(fd, &ClientsWithInput);
	YieldControl();
    }
    else
	YieldControlNoInput();
}

    /* lookup table for adding padding bytes to data that is read from
    	or written to the X socket.  */
static int padlength[4] = {0, 3, 2, 1};

 /********************
 * FlushClient()
 *    If the client isn't keeping up with us, then we try to continue
 *    buffering the data and set the apropriate bit in ClientsWritable
 *    (which is used by WaitFor in the select).  If the connection yields
 *    a permanent error, or we can't allocate any more space, we then
 *    close the connection.
 *
 **********************/

int
StandardFlushClient(who, oc, extraBuf, extraCount)
    ClientPtr who;
    OsCommPtr oc;
    char *extraBuf;
    int extraCount; /* do not modify... returned below */
{
    register ConnectionOutputPtr oco = oc->output;
    int connection = oc->fd;
    XtransConnInfo trans_conn = oc->trans_conn;
    struct iovec iov[3];
    static char padBuffer[3];
    long written;
    long padsize;
    long notWritten;
    long todo;

    if (!oco)
	return 0;
    written = 0;
    padsize = padlength[extraCount & 3];
    notWritten = oco->count + extraCount + padsize;
    todo = notWritten;
    while (notWritten) {
	long before = written;	/* amount of whole thing written */
	long remain = todo;	/* amount to try this time, <= notWritten */
	int i = 0;
	long len;

	/* You could be very general here and have "in" and "out" iovecs
	 * and write a loop without using a macro, but what the heck.  This
	 * translates to:
	 *
	 *     how much of this piece is new?
	 *     if more new then we are trying this time, clamp
	 *     if nothing new
	 *         then bump down amount already written, for next piece
	 *         else put new stuff in iovec, will need all of next piece
	 *
	 * Note that todo had better be at least 1 or else we'll end up
	 * writing 0 iovecs.
	 */
#define InsertIOV(pointer, length) \
	len = (length) - before; \
	if (len > remain) \
	    len = remain; \
	if (len <= 0) { \
	    before = (-len); \
	} else { \
	    iov[i].iov_len = len; \
	    iov[i].iov_base = (pointer) + before; \
	    i++; \
	    remain -= len; \
	    before = 0; \
	}

	InsertIOV ((char *)oco->buf, oco->count)
	InsertIOV (extraBuf, extraCount)
	InsertIOV (padBuffer, padsize)

	errno = 0;
	if (trans_conn)
	    len = _LBXPROXYTransWritev(trans_conn, iov, i);
	else
	    /*
	     * trans_conn can be NULL if the read is from an X server
	     */
	    len = (*oc->Writev) (connection, iov, i);
	if (len >= 0)
	{
	    written += len;
	    notWritten -= len;
	    todo = notWritten;
	}
	else if (ETEST(errno)
#ifdef SUNSYSV /* check for another brain-damaged OS bug */
		 || (errno == 0)
#endif
#ifdef EMSGSIZE /* check for another brain-damaged OS bug */
		 || ((errno == EMSGSIZE) && (todo == 1))
#endif
		)
	{
	    /* If we've arrived here, then the client is stuffed to the gills
	       and not ready to accept more.  Make a note of it and buffer
	       the rest. */
	    FD_SET(connection, &ClientsWriteBlocked);
	    AnyClientsWriteBlocked = TRUE;

	    if (written < oco->count)
	    {
		if (written > 0)
		{
		    oco->count -= written;
		    memmove((char *)oco->buf,
			    (char *)oco->buf + written,
			  oco->count);
		    written = 0;
		}
	    }
	    else
	    {
		written -= oco->count;
		oco->count = 0;
	    }

	    if (notWritten > oco->size)
	    {
		unsigned char *obuf;

		obuf = (unsigned char *)xrealloc(oco->buf,
						 notWritten + BUFSIZE);
		if (!obuf)
		{
		    _LBXPROXYTransDisconnect(oc->trans_conn);
		    _LBXPROXYTransClose(oc->trans_conn);
		    oc->trans_conn = NULL;
		    MarkClientException(who);
		    oco->count = 0;
		    return(-1);
		}
		oco->size = notWritten + BUFSIZE;
		oco->buf = obuf;
	    }

	    /* If the amount written extended into the padBuffer, then the
	       difference "extraCount - written" may be less than 0 */
	    if ((len = extraCount - written) > 0)
		memmove ((char *)oco->buf + oco->count,
			 extraBuf + written,
		       len);

	    oco->count = notWritten; /* this will include the pad */
	    /* return only the amount explicitly requested */
	    return extraCount;
	}
#ifdef EMSGSIZE /* check for another brain-damaged OS bug */
	else if (errno == EMSGSIZE)
	{
	    todo >>= 1;
	}
#endif
	else
	{
	    if (oc->trans_conn)
	    {
		_LBXPROXYTransDisconnect(oc->trans_conn);
		_LBXPROXYTransClose(oc->trans_conn);
		oc->trans_conn = NULL;
	    }
	    MarkClientException(who);
	    oco->count = 0;
	    return(-1);
	}
    }

    /* everything was flushed out */
    oco->count = 0;
    /* check to see if this client was write blocked */
    if (AnyClientsWriteBlocked)
    {
	FD_CLR(oc->fd, &ClientsWriteBlocked);
 	if (! XFD_ANYSET(&ClientsWriteBlocked))
	    AnyClientsWriteBlocked = FALSE;
    }
    if (oco->size > BUFWATERMARK)
    {
	xfree(oco->buf);
	xfree(oco);
    }
    else
    {
	oco->next = FreeOutputs;
	FreeOutputs = oco;
    }
    oc->output = (ConnectionOutputPtr)NULL;
    return extraCount; /* return only the amount explicitly requested */
}

static int
ExpandOutputBuffer(oco, len)
    ConnectionOutputPtr oco;
    int len;
{
    unsigned char *obuf;

    if (len < BUFSIZE)
	len = BUFSIZE;
    obuf = (unsigned char *)xrealloc(oco->buf, len);
    if (!obuf)
    {
	oco->count = 0;
	return(-1);
    }
    oco->size = len;
    oco->buf = obuf;
    return 0;
}

int
LbxFlushClient(who, oc, extraBuf, extraCount)
    ClientPtr who;
    OsCommPtr oc;
    char *extraBuf;
    int extraCount; /* do not modify... returned below */
{
    ConnectionOutputPtr obuf;
    register ConnectionOutputPtr oco;
    int retval;

    if ((oco = oc->ofirst)) {
	obuf = oc->output;
	do {
	    Bool nocomp = oco->nocompress;
	    oc->output = oco;
	    oco = (oco != oc->olast) ? oco->next : NULL;
	    if (nocomp)
		(*oc->compressOff)(oc->fd);
	    retval = StandardFlushClient(who, oc, (char *)NULL, 0);
	    if (nocomp)
		(*oc->compressOn)(oc->fd);
	    if (retval < 0) {
		oc->output = obuf;
		return retval;
	    }
	    if (oc->output) {
		if (extraCount) {
		    int len = (obuf->count + extraCount + 3) & ~3;
		    if (ExpandOutputBuffer(obuf, len) < 0) {
			if (oc->trans_conn) {
			    _LBXPROXYTransDisconnect(oc->trans_conn);
			    _LBXPROXYTransClose(oc->trans_conn);
			    oc->trans_conn = NULL;
			}
			MarkClientException(who);
			return(-1);
		    }
		    memmove((char *)obuf->buf + obuf->count,
			    extraBuf, extraCount);
		    obuf->count = len;
		    oc->olast->next = obuf;
		    oc->olast = obuf;
		    obuf = NULL;
		}
		oc->output = obuf;
		return extraCount;
	    }
	} while ((oc->ofirst = oco));
	oc->output = obuf;
    }
    retval = StandardFlushClient(who, oc, extraBuf, extraCount);
    if (retval <= 0)
	return retval;
    if (oc->output && extraCount) {
	oc->ofirst = oc->olast = oc->output;
	oc->output = NULL;
    }
    return retval;
}

 /********************
 * FlushAllOutput()
 *    Flush all clients with output.  However, if some client still
 *    has input in the queue (more requests), then don't flush.  This
 *    will prevent the output queue from being flushed every time around
 *    the round robin queue.  Now, some say that it SHOULD be flushed
 *    every time around, but...
 *
 **********************/

void
FlushAllOutput()
{
    register int index, base;
    register fd_mask mask;
    OsCommPtr oc;
    register ClientPtr client;

    if (! NewOutputPending)
	return;

    /*
     * It may be that some client still has critical output pending,
     * but he is not yet ready to receive it anyway, so we will
     * simply wait for the select to tell us when he's ready to receive.
     */
    NewOutputPending = FALSE;

    for (base = 0; base < howmany(XFD_SETSIZE, NFDBITS); base++)
    {
	mask = OutputPending.fds_bits[ base ];
	OutputPending.fds_bits[ base ] = 0;
	while (mask)
	{
	    index = ffs(mask) - 1;
	    mask &= ~lowbit(mask);
	    if ((index = ConnectionOutputTranslation[(base * (sizeof(fd_mask)*8)) + index]) == 0)
		continue;
	    client = clients[index];
	    if (client->clientGone)
		continue;
	    oc = (OsCommPtr)client->osPrivate;
	    if (FD_ISSET(oc->fd, &ClientsWithInput))
	    {
		FD_SET(oc->fd, &OutputPending); /* set the bit again */
		NewOutputPending = TRUE;
	    }
	    else
		(void)FlushClient(client, oc, (char *)NULL, 0);
	}
    }

}

/*****************
 * WriteToClient
 *    Copies buf into ClientPtr.buf if it fits (with padding), else
 *    flushes ClientPtr.buf and buf to client.  As of this writing,
 *    every use of WriteToClient is cast to void, and the result
 *    is ignored.  Potentially, this could be used by requests
 *    that are sending several chunks of data and want to break
 *    out of a loop on error.  Thus, we will leave the type of
 *    this routine as int.
 *****************/

int
StandardWriteToClient (who, count, buf)
    ClientPtr who;
    char *buf;
    int count;
{
    OsCommPtr oc = (OsCommPtr)who->osPrivate;
    register ConnectionOutputPtr oco = oc->output;
    int padBytes;

    if (!count)
	return(0);

    if (!oco)
    {
	if ((oco = FreeOutputs))
	{
	    FreeOutputs = oco->next;
	}
	else if (!(oco = AllocateOutputBuffer()))
	{
	    if (oc->trans_conn) 
	    {
		_LBXPROXYTransDisconnect(oc->trans_conn);
		_LBXPROXYTransClose(oc->trans_conn);
		oc->trans_conn = NULL;
	    }
	    MarkClientException(who);
	    return -1;
	}
	oc->output = oco;
    }

    padBytes =  padlength[count & 3];

    if (oco->count + count + padBytes > oco->size)
    {
	FD_CLR(oc->fd, &OutputPending);
	NewOutputPending = FALSE;
	return FlushClient(who, oc, buf, count);
    }

    NewOutputPending = TRUE;
    FD_SET(oc->fd, &OutputPending);
    memmove((char *)oco->buf + oco->count, buf, count);
    oco->count += count;

    if (padBytes)
    {
	bzero ((char *)oco->buf + oco->count, padBytes);
	oco->count += padBytes;
    }
    return(count);
}

int
UncompressWriteToClient (who, count, buf)
    ClientPtr who;
    char *buf;
    int count;
{
    OsCommPtr oc = (OsCommPtr)who->osPrivate;
    register ConnectionOutputPtr oco;
    int paddedLen = count + padlength[count & 3];

    if (!count)
	return(0);

    if ((oco = oc->output)) {
	/*
	 * we're currently filling a buffer, and it must be compressible,
	 * so put it on the queue
	 */
	if (oc->ofirst) {
	    oc->olast->next = oco;
	    oc->olast = oco;
	}
	else {
	    oc->ofirst = oc->olast = oco;
	}
	oco = oc->output = (ConnectionOutputPtr)NULL;
    }
    else if (oc->ofirst) {
	oco = oc->olast;
	if (!oco->nocompress || ((oco->count + paddedLen) > oco->size))
	    oco = (ConnectionOutputPtr)NULL;
    }

    if (!oco) {
	/*
	 * First try to use the FreeOutputs buffer
	 */
	oco = FreeOutputs;
	if (oco) {
	    FreeOutputs = oco->next;

	    oco->next = 0;
	    oco->count = 0;
	    oco->nocompress = TRUE;

	    if (oco->size < paddedLen) {
		oco->buf = (unsigned char *) xrealloc (oco->buf, paddedLen);
		if (!oco->buf) {
	            if (oc->trans_conn) {
			_LBXPROXYTransDisconnect(oc->trans_conn);
			_LBXPROXYTransClose(oc->trans_conn);
			oc->trans_conn = NULL;
		    }
		    MarkClientException(who);
		    return -1;
		}
		oco->size = paddedLen;
	    }
	}
	else if (!(oco = AllocateUncompBuffer(paddedLen))) {
	    if (oc->trans_conn) {
		_LBXPROXYTransDisconnect(oc->trans_conn);
		_LBXPROXYTransClose(oc->trans_conn);
		oc->trans_conn = NULL;
	    }
	    MarkClientException(who);
	    return -1;
	}
    }
    memmove((char *)oco->buf + oco->count, buf, count);
    oco->count += paddedLen;

    if (oc->ofirst) {
	oc->olast->next = oco;
	oc->olast = oco;
    }
    else {
	oc->ofirst = oc->olast = oco;
    }

    NewOutputPending = TRUE;
    FD_SET(oc->fd, &OutputPending);
    return(count);
}

static ConnectionInputPtr
AllocateInputBuffer()
{
    register ConnectionInputPtr oci;

    oci = (ConnectionInputPtr)xalloc(sizeof(ConnectionInput));
    if (!oci)
	return (ConnectionInputPtr)NULL;
    oci->buffer = (char *)xalloc(BUFSIZE);
    if (!oci->buffer)
    {
	xfree(oci);
	return (ConnectionInputPtr)NULL;
    }
    oci->size = BUFSIZE;
    oci->bufptr = oci->buffer;
    oci->bufcnt = 0;
    oci->lenLastReq = 0;
    return oci;
}

static ConnectionOutputPtr
AllocateOutputBuffer()
{
    register ConnectionOutputPtr oco;

    oco = (ConnectionOutputPtr)xalloc(sizeof(ConnectionOutput));
    if (!oco)
	return (ConnectionOutputPtr)NULL;
    oco->buf = (unsigned char *) xalloc(BUFSIZE);
    if (!oco->buf)
    {
	xfree(oco);
	return (ConnectionOutputPtr)NULL;
    }
    oco->size = BUFSIZE;
    oco->count = 0;
    oco->nocompress = FALSE;
    return oco;
}

static ConnectionOutputPtr
AllocateUncompBuffer(count)
    int count;
{
    register ConnectionOutputPtr oco;
    int len = (count > BUFSIZE) ? count : BUFSIZE;

    oco = (ConnectionOutputPtr)xalloc(sizeof(ConnectionOutput));
    if (!oco)
	return (ConnectionOutputPtr)NULL;
    oco->buf = (unsigned char *) xalloc(len);
    if (!oco->buf)
    {
	xfree(oco);
	return (ConnectionOutputPtr)NULL;
    }
    oco->size = len;
    oco->count = 0;
    oco->nocompress = TRUE;
    return oco;
}

void
FreeOsBuffers(oc)
    OsCommPtr oc;
{
    register ConnectionInputPtr oci;
    register ConnectionOutputPtr oco;

    if (AvailableInput == oc)
	AvailableInput = (OsCommPtr)NULL;
    if ((oci = oc->input))
    {
	if (FreeInputs)
	{
	    xfree(oci->buffer);
	    xfree(oci);
	}
	else
	{
	    FreeInputs = oci;
	    oci->next = (ConnectionInputPtr)NULL;
	    oci->bufptr = oci->buffer;
	    oci->bufcnt = 0;
	    oci->lenLastReq = 0;
	}
    }
    if ((oco = oc->output))
    {
	if (FreeOutputs)
	{
	    xfree(oco->buf);
	    xfree(oco);
	}
	else
	{
	    FreeOutputs = oco;
	    oco->next = (ConnectionOutputPtr)NULL;
	    oco->count = 0;
	}
    }
    if ((oco = oc->ofirst)) {
	ConnectionOutputPtr nextoco;
	do {
	    nextoco = oco->next;
	    xfree(oco->buf);
	    xfree(oco);
	    if (oco == oc->olast)
		break;
	    oco = nextoco;
	} while (1);
    }
}
