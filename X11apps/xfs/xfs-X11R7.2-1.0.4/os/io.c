/* $Xorg: io.c,v 1.5 2001/02/09 02:05:44 xorgcvs Exp $ */
/*
 * i/o functions
 */
/*
 
Copyright 1990, 1991, 1998  The Open Group

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

 * Copyright 1990, 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation 
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices, or Digital
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES,
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */
/* $XFree86: xc/programs/xfs/os/io.c,v 3.18 2001/12/14 20:01:41 dawes Exp $ */

#include	<X11/Xtrans/Xtrans.h>
#include	<stdio.h>
#include	<errno.h>
#include	<sys/types.h>
#ifndef Lynx
#include	<sys/param.h>
#ifndef __UNIXOS2__
#include	<sys/uio.h>
#endif
#else
#include	<uio.h>
#endif

#include	<X11/fonts/FSproto.h>
#include	"clientstr.h"
#include	"X11/Xpoll.h"
#include	"osdep.h"
#include	"globals.h"
#include	"dispatch.h"


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


extern fd_set ClientsWithInput;
extern fd_set ClientsWriteBlocked;
extern fd_set OutputPending;

extern long OutputBufferSize;

extern int  ConnectionTranslation[];

extern Bool AnyClientsWriteBlocked;
extern Bool NewOutputPending;

static int  timesThisConnection = 0;
static ConnectionInputPtr FreeInputs = (ConnectionInputPtr) NULL;
static ConnectionOutputPtr FreeOutputs = (ConnectionOutputPtr) NULL;
static OsCommPtr AvailableInput = (OsCommPtr) NULL;

static ConnectionInputPtr AllocateInputBuffer(void);
static ConnectionOutputPtr AllocateOutputBuffer(void);


#define		MAX_TIMES_PER	10

#define	yield_control()				\
	{ isItTimeToYield = TRUE;		\
	  timesThisConnection = 0; }

#define	yield_control_no_input()		\
	{ yield_control();			\
	  FD_CLR(fd, &ClientsWithInput); }

#define	yield_control_death()			\
	{ timesThisConnection = 0; }

#define	request_length(req, client)		\
	((int)((client)->swapped ? lswaps((req)->length) : (req)->length) << 2)

int
ReadRequest(ClientPtr client)
{
    OsCommPtr   oc;
    ConnectionInputPtr oci;
    fsReq      *request;
    int         fd,
                result,
                gotnow,
                needed = 0;

    if (client == NULL)
	return -1;
    oc = (OsCommPtr) client->osPrivate;
    if (oc == NULL)
	return -1;
    oci = oc->input;
    fd = oc->fd;
    if (oci != NULL && fd < 0)
	return -1;
		
    if (AvailableInput) {
	if (AvailableInput != oc) {
	    ConnectionInputPtr aci = AvailableInput->input;

	    if (aci->size > BUFWATERMARK) {
		fsfree(aci->buffer);
		fsfree(aci);
	    } else {
		aci->next = FreeInputs;
		FreeInputs = aci;
	    }
	    AvailableInput->input = (ConnectionInputPtr) NULL;
	}
	AvailableInput = (OsCommPtr) NULL;
    }
    if (!oci) {
	if ((oci = FreeInputs ) != (ConnectionInputPtr) 0) {
	    FreeInputs = oci->next;
	} else if (!(oci = AllocateInputBuffer())) {
	    yield_control_death();
	    return -1;
	}
	oc->input = oci;
    }
    oci->bufptr += oci->lenLastReq;

    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;

#ifdef WORD64
    /* need 8-byte alignment */
    if ((oci->bufptr - oci->buffer) & 7  &&  gotnow > 0)
    {
	memmove( oci->buffer, oci->bufptr, gotnow);
	oci->bufptr = oci->buffer;
	oci->bufcnt = gotnow;
    }
#endif

    request = (fsReq *) oci->bufptr;

    /* not enough for a request */
    if ((gotnow < SIZEOF(fsReq)) ||
	    (gotnow < (needed = request_length(request, client)))) {
	oci->lenLastReq = 0;
	if ((gotnow < SIZEOF(fsReq)) || needed == 0)
	    needed = SIZEOF(fsReq);
	else if (needed > MAXBUFSIZE) {
	    yield_control_death();
	    return -1;
	}
	/* see if we need to shift up a partial request so the rest can fit */
	if ((gotnow == 0) ||
	    ((oci->bufptr - oci->buffer + needed) > oci->size))
	{
	    if ((gotnow > 0) && (oci->bufptr != oci->buffer))
		memmove( oci->buffer, oci->bufptr, gotnow);
	    /* grow buffer if necessary */
	    if (needed > oci->size) {
		char       *ibuf;

		ibuf = (char *) fsrealloc(oci->buffer, needed);
		if (!ibuf) {
		    yield_control_death();
		    return -1;
		}
		oci->size = needed;
		oci->buffer = ibuf;
	    }
	    oci->bufptr = oci->buffer;
	    oci->bufcnt = gotnow;
	}
	/* fill 'er up */
	if (oc->trans_conn == NULL) {
	    yield_control_death();
	    return -1;
	}
	result = _FontTransRead(oc->trans_conn, oci->buffer + oci->bufcnt,
		      oci->size - oci->bufcnt);
	if (result <= 0) {
#if !(defined(SVR4) && defined(i386) && !defined(sun))
	    if ((result < 0) && ETEST(errno)) {
		yield_control_no_input();
		return 0;
	    } else
#endif
	    {

		yield_control_death();
		return -1;
	    }
	}
	oci->bufcnt += result;
	gotnow += result;

	/* free up space after huge requests */
	if ((oci->size > BUFWATERMARK) &&
		(oci->bufcnt < BUFSIZE) && (needed < BUFSIZE)) {
	    char       *ibuf;

	    ibuf = (char *) fsrealloc(oci->buffer, BUFSIZE);
	    if (ibuf) {
		oci->size = BUFSIZE;
		oci->buffer = ibuf;
		oci->bufptr = ibuf + oci->bufcnt - gotnow;
	    }
	}
	request = (fsReq *) oci->bufptr;
	if ((gotnow < SIZEOF(fsReq)) ||
	    (gotnow < (needed = request_length(request, client)))) {
	    yield_control_no_input();
	    return 0;
	}
    }
    if (needed == 0)
	needed = SIZEOF(fsReq);
    oci->lenLastReq = needed;
    /*
     * Check to see if client has at least one whole request in the buffer. If
     * there is only a partial request, treat like buffer is empty so that
     * select() will be called again and other clients can get into the queue.
     */

    if (gotnow >= needed + SIZEOF(fsReq)) {
	request = (fsReq *) (oci->bufptr + needed);
	if (gotnow >= needed + request_length(request, client))
	    FD_SET(fd, &ClientsWithInput);
	else
	    yield_control_no_input();
    } else {
	if (gotnow == needed)
	    AvailableInput = oc;
	yield_control_no_input();
    }

    if (++timesThisConnection >= MAX_TIMES_PER)
	yield_control();

    client->requestBuffer = (pointer) oci->bufptr;
    return needed;
}

Bool
InsertFakeRequest(ClientPtr client, char *data, int count)
{
    OsCommPtr   oc = (OsCommPtr) client->osPrivate;
    ConnectionInputPtr oci = oc->input;
    int         fd = oc->fd;
    fsReq      *request;
    int         gotnow,
                moveup;

    if (AvailableInput) {
	if (AvailableInput != oc) {
	    register ConnectionInputPtr aci = AvailableInput->input;

	    if (aci->size > BUFWATERMARK) {
		fsfree(aci->buffer);
		fsfree(aci);
	    } else {
		aci->next = FreeInputs;
		FreeInputs = aci;
	    }
	    AvailableInput->input = (ConnectionInputPtr) NULL;
	}
	AvailableInput = (OsCommPtr) NULL;
    }
    if (!oci) {
	if ((oci = FreeInputs) != (ConnectionInputPtr) 0)
	    FreeInputs = oci->next;
	else if (!(oci = AllocateInputBuffer()))
	    return FALSE;
	oc->input = oci;

    }
    oci->bufptr += oci->lenLastReq;
    oci->lenLastReq = 0;
    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;
    if ((gotnow + count) > oci->size) {
	char       *ibuf;

	ibuf = (char *) fsrealloc(oci->buffer, gotnow + count);
	if (!ibuf)
	    return FALSE;
	oci->size = gotnow + count;
	oci->buffer = ibuf;
	oci->bufptr = ibuf + oci->bufcnt - gotnow;
    }
    moveup = count - (oci->bufptr - oci->buffer);
    if (moveup > 0) {
	if (gotnow > 0)
	    memmove( oci->bufptr + moveup, oci->bufptr, gotnow);
	oci->bufptr += moveup;
	oci->bufcnt += moveup;
    }
    memmove( oci->bufptr - count, data, count);
    oci->bufptr -= count;
    request = (fsReq *) oci->bufptr;
    gotnow += count;
    if ((gotnow >= SIZEOF(fsReq)) &&
	    (gotnow >= request_length(request, client)))
	FD_SET(fd, &ClientsWithInput);
    else
	yield_control_no_input();
    return TRUE;
}

void
ResetCurrentRequest(ClientPtr client)
{
    OsCommPtr   oc = (OsCommPtr) client->osPrivate;
    ConnectionInputPtr oci = oc->input;
    int         fd = oc->fd;
    fsReq      *request;
    int         gotnow;

    if (AvailableInput == oc)
	AvailableInput = (OsCommPtr) NULL;
    oci->lenLastReq = 0;
    request = (fsReq *) oci->bufptr;
    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;
    if ((gotnow >= SIZEOF(fsReq)) &&
	    (gotnow >= request_length(request, client))) {
	FD_SET(fd, &ClientsWithInput);
	yield_control();
    } else {
	yield_control_no_input();
    }
}

int
FlushClient(
    ClientPtr   client,
    OsCommPtr   oc,
    char       *extraBuf,
    int         extraCount,
    int         padsize)
{
    ConnectionOutputPtr oco = oc->output;
    int         fd = oc->fd;
    struct iovec iov[3];
    char        padBuffer[3];
    long        written;
    long        notWritten;
    long        todo;

    if (!oco)
	return 0;
    written = 0;
    notWritten = oco->count + extraCount + padsize;
    todo = notWritten;
    while (notWritten) {
	long        before = written;
	long        remain = todo;
	int         i = 0;
	long        len;

	/*-
	 * You could be very general here and have "in" and "out" iovecs and
	 * write a loop without using a macro, but what the heck.  This
	 * translates to:
	 *
	 * 	how much of this piece is new?
	 *	if more new then we are trying this time, clamp
	 *	if nothing new
	 *	    then bump down amount already written, for next piece
	 *	    else put new stuff in iovec, will need all of next piece
	 *
	 * Note that todo had better be at least 1 or else we'll end up
	 * writing 0 iovecs.
	 */

#define	InsertIOV(pointer, length)	\
	len = (length) - before;	\
	if (len > remain)		\
	    len = remain;		\
	if (len <= 0) {			\
	    before = (-len);		\
	} else {			\
	    iov[i].iov_len = len;	\
	    iov[i].iov_base = (pointer) + before; \
	    i++;			\
	    remain -= len;		\
	    before = 0;			\
	}

	InsertIOV((char *) oco->buf, oco->count);
	InsertIOV(extraBuf, extraCount);
	InsertIOV(padBuffer, padsize);

	errno = 0;
	if (oc->trans_conn && (len = _FontTransWritev(oc->trans_conn, iov, i)) >= 0) {
	    written += len;
	    notWritten -= len;
	    todo = notWritten;
	} else if (ETEST(errno)
#ifdef SUNSYSV /* check for another brain-damaged OS bug */
		 || (errno == 0)
#endif
#ifdef EMSGSIZE /* check for another brain-damaged OS bug */
		 || ((errno == EMSGSIZE) && (todo == 1))
#endif
		)
	{
	    FD_SET(fd, &ClientsWriteBlocked);
	    AnyClientsWriteBlocked = TRUE;

	    if (written < oco->count) {
		if (written > 0) {
		    oco->count -= written;
		    memmove( (char *) oco->buf, (char *) oco->buf + written,
			    oco->count);
		    written = 0;
		}
	    } else {
		written -= oco->count;
		oco->count = 0;
	    }

	    /* grow buffer if necessary */
	    if (notWritten > oco->size) {
		unsigned char *obuf;

		obuf = (unsigned char *) fsrealloc(oco->buf,
					      notWritten + OutputBufferSize);
		if (!obuf) {
		    if (oc->trans_conn)
			_FontTransClose(oc->trans_conn);
		    oc->trans_conn = NULL;
		    MarkClientException(client);
		    oco->count = 0;
		    return -1;
		}
		oco->size = notWritten + OutputBufferSize;
		oco->buf = obuf;
	    }
	    if ((len = extraCount - written) > 0) {
		memmove( (char *) oco->buf + oco->count, 
			extraBuf + written, len);
	    }
	    oco->count = notWritten;
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
	        _FontTransClose(oc->trans_conn);
	    oc->trans_conn = NULL;
	    MarkClientException(client);
	    oco->count = 0;
	    return -1;
	}
    }

    /* everything was flushed */
    oco->count = 0;

    /* clear the write block if it was set */
    if (AnyClientsWriteBlocked) {
	FD_CLR(fd, &ClientsWriteBlocked);
	if (!XFD_ANYSET(&ClientsWriteBlocked))
	    AnyClientsWriteBlocked = FALSE;
    }
    if (oco->size > BUFWATERMARK) {
	fsfree(oco->buf);
	fsfree(oco);
    } else {
	oco->next = FreeOutputs;
	FreeOutputs = oco;
    }
    oc->output = (ConnectionOutputPtr) NULL;

    return extraCount;
}

void
FlushAllOutput(void)
{
    int         index, base;
    fd_mask	mask;
    OsCommPtr   oc;
    ClientPtr   client;

    if (!NewOutputPending)
	return;

    NewOutputPending = FALSE;

    for (base = 0; base < howmany(XFD_SETSIZE, NFDBITS); base++) {
	mask = OutputPending.fds_bits[base];
	OutputPending.fds_bits[base] = 0;
	while (mask) {
	    index = ffs(mask) - 1;
	    mask &= ~lowbit(mask);
	    if ((index = ConnectionTranslation[(base << 5) + index]) == 0)
		continue;
	    client = clients[index];
	    if (client->clientGone == CLIENT_GONE)
		continue;
	    oc = (OsCommPtr) client->osPrivate;
	    if (FD_ISSET(oc->fd, &ClientsWithInput)) {
		FD_SET(oc->fd, &OutputPending);
		NewOutputPending = TRUE;
	    } else {
		(void) FlushClient(client, oc, (char *) NULL, 0, 0);
	    }
	}
    }
}

/*
 * returns number of bytes written
 */
static int
write_to_client_internal(ClientPtr client, int count, char *buf, int padBytes)
{
    OsCommPtr   oc = (OsCommPtr) client->osPrivate;
    ConnectionOutputPtr oco = oc->output;

    if (!count)
	return 0;

    if (!oco) {
	if ((oco = FreeOutputs) != (ConnectionOutputPtr) 0) {
	    FreeOutputs = oco->next;
	} else if (!(oco = AllocateOutputBuffer())) {
	    _FontTransClose(oc->trans_conn);
	    oc->trans_conn = NULL;
	    MarkClientException(client);
	    return -1;
	}
	oc->output = oco;
    }
    if (oco->count + count + padBytes > oco->size) {
	FD_CLR(oc->fd, &OutputPending);
	NewOutputPending = FALSE;
	return FlushClient(client, oc, buf, count, padBytes);
    }
    NewOutputPending = TRUE;
    FD_SET(oc->fd, &OutputPending);
    memmove( (char *) oco->buf + oco->count, buf, count);
    oco->count += count + padBytes;

    return count;
}

void
WriteToClientUnpadded(ClientPtr client, int count, char *buf)
{
    write_to_client_internal(client, count, buf, 0);
}

static int  padlength[4] = {0, 3, 2, 1};

void
WriteToClient(ClientPtr client, int count, char *buf)
{
    int flag = 0;
    if (NULL == buf) {
	flag = -1;
	buf = (char *)fsalloc(count); memset(buf, 0, count);
    }
     write_to_client_internal(client, count, buf, padlength[count & 3]);
    if (flag)
	fsfree(buf);
}

static ConnectionInputPtr
AllocateInputBuffer(void)
{
    register ConnectionInputPtr oci;

    oci = (ConnectionInputPtr) fsalloc(sizeof(ConnectionInput));
    if (!oci)
	return (ConnectionInputPtr) NULL;
    oci->buffer = (char *) fsalloc(BUFSIZE);
    if (!oci->buffer) {
	fsfree(oci);
	return (ConnectionInputPtr) NULL;
    }
    oci->next = 0;
    oci->size = BUFSIZE;
    oci->bufptr = oci->buffer;
    oci->bufcnt = 0;
    oci->lenLastReq = 0;
    return oci;
}

static ConnectionOutputPtr
AllocateOutputBuffer(void)
{
    register ConnectionOutputPtr oco;

    oco = (ConnectionOutputPtr) fsalloc(sizeof(ConnectionOutput));
    if (!oco)
	return (ConnectionOutputPtr) NULL;
    oco->buf = (unsigned char *) fsalloc(BUFSIZE);
    if (!oco->buf) {
	fsfree(oco);
	return (ConnectionOutputPtr) NULL;
    }
    oco->size = BUFSIZE;
    oco->count = 0;
    return oco;
}


void
FreeOsBuffers(OsCommPtr oc)
{
    register ConnectionInputPtr oci;
    register ConnectionOutputPtr oco;

    if (AvailableInput == oc)
	AvailableInput = (OsCommPtr) NULL;
    if ((oci = oc->input) != (ConnectionInputPtr) 0) {
	if (FreeInputs) {
	    fsfree(oci->buffer);
	    fsfree(oci);
	} else {
	    FreeInputs = oci;
	    oci->next = (ConnectionInputPtr) NULL;
	    oci->bufptr = oci->buffer;
	    oci->bufcnt = 0;
	    oci->lenLastReq = 0;
	}
    }
    if ((oco = oc->output) != (ConnectionOutputPtr) 0) {
	if (FreeOutputs) {
	    fsfree(oco->buf);
	    fsfree(oco);
	} else {
	    FreeOutputs = oco;
	    oco->next = (ConnectionOutputPtr) NULL;
	    oco->count = 0;
	}
    }
}

void
ResetOsBuffers(void)
{
    register ConnectionInputPtr oci;
    register ConnectionOutputPtr oco;

    while ((oci = FreeInputs) != (ConnectionInputPtr) 0) {
	FreeInputs = oci->next;
	fsfree(oci->buffer);
	fsfree(oci);
    }
    while ((oco = FreeOutputs) != (ConnectionOutputPtr) 0) {
	FreeOutputs = oco->next;
	fsfree(oco->buf);
	fsfree(oco);
    }
}
