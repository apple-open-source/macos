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

#ifndef	_CLIENT_H_
#define	_CLIENT_H_

#ifndef _XTYPEDEF_CLIENTPTR
typedef struct _Client *ClientPtr;
#define _XTYPEDEF_CLIENTPTR
#endif

#include <misc.h>

extern ClientPtr *clients;
extern ClientPtr serverClient;

#define	NullClient	((ClientPtr) NULL)

#define	SERVER_CLIENT	0
#define	MINCLIENT	1

#define	CLIENT_ALIVE		0
#define	CLIENT_GONE		1
#define	CLIENT_AGED		2
#define	CLIENT_TIMED_OUT	4

#define	REQUEST(type)	\
	type *stuff = (type *)client->requestBuffer

#define	REQUEST_FIXED_SIZE(fs_req, n)					\
	if (((SIZEOF(fs_req) >> 2) > stuff->length) ||			\
		(((SIZEOF(fs_req) + (n) + 3) >> 2) != stuff->length)) {	\
	    int lengthword = stuff->length;				\
	    SendErrToClient(client, FSBadLength, (pointer)&lengthword); \
	    return (FSBadLength);	\
	}

#define	REQUEST_SIZE_MATCH(fs_req)				\
	if ((SIZEOF(fs_req) >> 2) != stuff->length) {	\
	    int lengthword = stuff->length;				\
	    SendErrToClient(client, FSBadLength, (pointer)&lengthword); \
	    return (FSBadLength);	\
	}

#define	REQUEST_AT_LEAST_SIZE(fs_req)					\
	if ((SIZEOF(fs_req) >> 2) > stuff->length) {			\
	    int lengthword = stuff->length;				\
	    SendErrToClient(client, FSBadLength, (pointer)&lengthword); \
	    return (FSBadLength);	\
	}

#define	WriteReplyToClient(client, size, reply)			\
	if ((client)->swapped)						\
	    (*ReplySwapVector[((fsReq *)(client)->requestBuffer)->reqType]) \
		(client, (int)(size), reply);				\
	else	(void)WriteToClient(client, (int)(size), (char *)(reply));

#define	WriteSwappedDataToClient(client, size, pbuf)		\
	if ((client)->swapped)						\
	    (*(client)->pSwapReplyFunc)(client, (int)(size), pbuf);	\
	else (void) WriteToClient(client, (int)(size), (char *)(pbuf));


typedef struct _WorkQueue       *WorkQueuePtr;

#include <difsutils.h> /* for ProcessWorkQueue(void) */

extern int  (*ProcVector[]) (ClientPtr);
extern int  (*SwappedProcVector[]) (ClientPtr);

#endif				/* _CLIENT_H_ */
