/* $Xorg: clientstr.h,v 1.4 2001/02/09 02:05:43 xorgcvs Exp $ */
/*
Copyright 1987, 1998  The Open Group

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
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices,
 * or Digital not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Network Computing Devices, or Digital
 * make no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES, OR DIGITAL BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/* $XFree86: xc/programs/xfs/include/clientstr.h,v 1.7 2001/12/14 20:01:37 dawes Exp $ */

#ifndef _CLIENTSTR_H_
#define	_CLIENTSTR_H_

#include	"FS.h"
#include	"client.h"
#include	"auth.h"
#include	"misc.h"

typedef struct _Client {
    int         index;
    pointer     osPrivate;
    int         noClientException;
    int         (**requestVector) (ClientPtr);
    pointer     requestBuffer;
    int         clientGone;
    int         sequence;
    Bool        swapped;
    long        last_request_time;
    void        (*pSwapReplyFunc) (ClientPtr, int, short *);
    AuthContextPtr auth;
    int		auth_generation;
    AuthContextPtr default_auth;
    char       *catalogues;
    int         num_catalogues;
    Mask        eventmask;
    fsResolution *resolutions;
    int         num_resolutions;
    int		major_version;	/* client-major-protocol-version */
    int		minor_version;
}           ClientRec;

typedef struct _WorkQueue {
    struct _WorkQueue *next;
    Bool        (*function) (ClientPtr, pointer);
    ClientPtr   client;
    pointer     closure;
}           WorkQueueRec;

#ifndef CloseDownClient
#ifdef DEBUG
#define CloseDownClient(client)	\
	fprintf(stderr, "CloseDownClient %s %d\n", __FILE__, __LINE__),\
	DoCloseDownClient(client)
#else
#define CloseDownClient DoCloseDownClient
#endif
extern void DoCloseDownClient(ClientPtr client);
#endif

#endif				/* _CLIENTSTR_H_ */
