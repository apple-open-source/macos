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
 *
 */

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include	<X11/fonts/FSproto.h>	/* for fsResolution */
#include	<X11/Xtrans/Xtrans.h>	/* XtransConnInfo */
#include        <X11/fonts/font.h>	/* FontPatternCachePtr */
#include	<X11/Xpoll.h>		/* select & fd_set */
#include	"client.h"		/* WorkQueuePtr */
#include	"misc.h"		/* OldListenRec */

extern long TimeOutValue;
extern long ReapClientTime;

extern int  currentMaxClients;
extern long MaxClients;
extern int  serverGeneration;

extern volatile char isItTimeToYield;
extern volatile char dispatchException;

extern int  argcGlobal;
extern char **argvGlobal;

extern WorkQueuePtr workQueue;

extern fd_set WellKnownConnections;
extern fd_set LastSelectMask;
extern fd_set WriteMask;
extern fd_set ClientsWithInput;
extern fd_set ClientsWriteBlocked;
extern fd_set AllSockets;
extern fd_set AllClients;
extern fd_set OutputPending;

extern Bool AnyClientsWriteBlocked;
extern Bool NewOutputPending;

extern int ConnectionTranslation[];

extern FontPatternCachePtr fontPatternCache;

extern long LastReapTime;
extern Bool drone_server;
extern char *progname;
extern char *configfilename;

extern XtransConnInfo *ListenTransConns;
extern int *ListenTransFds;
extern int  ListenTransCount;

extern OldListenRec *OldListen;
extern int OldListenCount;

extern Bool portFromCmdline;

extern char *ConnectionInfo;
extern int ConnInfoLen;

extern long OutputBufferSize;

/* bit values for dispatchException */
#define	DE_RESET	0x1
#define	DE_TERMINATE	0x2
#define	DE_RECONFIG	0x4
#define	DE_FLUSH	0x8

/* size of vector tables */
#define	NUM_PROC_VECTORS	25
#define	NUM_EVENT_VECTORS	8
#endif				/* _GLOBALS_H_ */
