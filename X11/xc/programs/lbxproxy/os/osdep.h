/* $Xorg: osdep.h,v 1.4 2001/02/09 02:05:33 xorgcvs Exp $ */
/***********************************************************

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


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

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
/* $XFree86: xc/programs/lbxproxy/os/osdep.h,v 1.11 2002/05/31 18:46:08 dawes Exp $ */

#define BOTIMEOUT 200 /* in milliseconds */
#define BUFSIZE 4096
#define BUFWATERMARK 8192
#define MAXBUFSIZE (1 << 22)

#include <X11/Xmd.h>

#ifndef sgi	    /* SGI defines OPEN_MAX in a useless way */
#ifndef X_NOT_POSIX
#ifdef _POSIX_SOURCE
#include <limits.h>
#else
#define _POSIX_SOURCE
#include <limits.h>
#undef _POSIX_SOURCE
#endif
#endif
#endif

#if defined(__UNIXOS2__) || defined(__QNX__)
#define OPEN_MAX 256
#endif

#ifndef OPEN_MAX
#ifdef SVR4
#define OPEN_MAX 128
#else
#include <sys/param.h>
#ifndef OPEN_MAX
#if defined(NOFILE) && !defined(NOFILES_MAX)
#define OPEN_MAX NOFILE
#else
#define OPEN_MAX NOFILES_MAX
#endif
#endif
#endif
#endif

#include <X11/Xpoll.h>

/*
 * MAXSOCKS is used only for initialising MaxClients when no other method
 * like sysconf(_SC_OPEN_MAX) is not supported.
 */

#if OPEN_MAX <= 128
#define MAXSOCKS (OPEN_MAX - 1)
#else
#define MAXSOCKS 128
#endif

/* MAXSELECT is the number of fds that select() can handle */
#define MAXSELECT (sizeof(fd_set) * NBBY)

#if !defined(hpux) && !defined(SVR4) && !defined(SYSV)
#define HAS_GETDTABLESIZE
#endif

#include <stddef.h>

typedef struct _connectionInput {
    struct _connectionInput *next;
    char *buffer;               /* contains current client input */
    char *bufptr;               /* pointer to current start of data */
    int  bufcnt;                /* count of bytes in buffer */
    int lenLastReq;
    int size;
} ConnectionInput, *ConnectionInputPtr;

typedef struct _connectionOutput {
    struct _connectionOutput *next;
    int size;
    unsigned char *buf;
    int count;
    Bool nocompress;
} ConnectionOutput, *ConnectionOutputPtr;

typedef struct _osComm {
    int fd;
    ConnectionInputPtr input;
    ConnectionOutputPtr output;
    ConnectionOutputPtr ofirst;
    ConnectionOutputPtr olast;
    void (*Close) ();
    int  (*Writev) ();
    int  (*Read) ();
    int  (*flushClient) ();
    void (*compressOff) ();
    void (*compressOn) ();
    struct _XtransConnInfo *trans_conn; /* transport connection object */
} OsCommRec, *OsCommPtr;

#define FlushClient(who, oc, extraBuf, extraCount) \
    (*((OsCommPtr)((who)->osPrivate))->flushClient)(who, oc, extraBuf, extraCount)

extern void FreeOsBuffers(
    OsCommPtr /*oc*/
);

extern int StandardFlushClient(
    ClientPtr /*who*/,
    OsCommPtr /*oc*/,
    char * /*extraBuf*/,
    int /*extraCount*/
);

extern int LbxFlushClient(
    ClientPtr /*who*/,
    OsCommPtr /*oc*/,
    char * /*extraBuf*/,
    int /*extraCount*/
);

#include "util.h"

extern int *ConnectionTranslation;
extern int *ConnectionOutputTranslation;

extern WorkQueuePtr workQueue;

#define ffs mffs
extern int mffs(fd_mask);

