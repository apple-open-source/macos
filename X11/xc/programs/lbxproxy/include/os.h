/* $Xorg: os.h,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */

/*

Copyright 1995, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/lbxproxy/include/os.h,v 1.7 2001/12/14 20:00:55 dawes Exp $ */

#ifndef OS_H
#define OS_H

#include <X11/Xtrans.h>

extern fd_set AllSockets;
extern fd_set AllClients;
extern fd_set LastSelectMask;
extern fd_set WellKnownConnections;
extern fd_set ClientsWithInput;
extern fd_set ClientsWriteBlocked;
extern fd_set OutputPending;

/* WaitFor.c */

extern int WaitForSomething(
#if NeedFunctionPrototypes
    int * /*pClientsReady*/,
    Bool  /* poll */
#endif
);

/* connection.c */

extern Bool NewOutputPending;
extern Bool AnyClientsWriteBlocked;
extern int GrabInProgress;

extern void InitConnectionLimits(
#if NeedFunctionPrototypes
    void
#endif
);

extern void CreateServerSockets(
#if NeedFunctionPrototypes
    int * /*listen_fds[]*/
#endif
);

extern void CloseServerSockets(
#if NeedFunctionPrototypes
    void
#endif
);

extern void CreateWellKnownSockets(
#if NeedFunctionPrototypes
    void
#endif
);

extern void ListenToProxyManager (
#if NeedFunctionPrototypes
    void
#endif
);

extern void ListenWellKnownSockets (
#if NeedFunctionPrototypes
    void
#endif
);

extern void ResetWellKnownSockets(
#if NeedFunctionPrototypes
    void
#endif
);

extern void AvailableClientInput(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern ClientPtr AllocNewConnection(
#if NeedFunctionPrototypes
    int /*fd*/,			/* the fd to the display */
    int /*connect_fd*/,		/* the fd the client connected on */
    Bool /*to_server*/,
    XtransConnInfo /*trans_conn*/
#endif
);

extern void SwitchConnectionFuncs(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    int (* /*Read*/)(),
    int (* /*Writev*/)()
#endif
);

extern void StartOutputCompression(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    void (* /*CompressOn*/)(),
    void (* /*CompressOff*/)()
#endif
);

extern Bool EstablishNewConnections(
#if NeedFunctionPrototypes
    ClientPtr /*clientUnused*/,
    pointer /*closure*/
#endif
);

extern void CloseDownFileDescriptor(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void CheckConnections(
#if NeedFunctionPrototypes
    void
#endif
);

extern void CloseDownConnection(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void OnlyListenToOneClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void ListenToAllClients(
#if NeedFunctionPrototypes
    void
#endif
);

extern void IgnoreClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void AttendClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void MakeClientGrabImpervious(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void MakeClientGrabPervious(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

/* io.c */

extern unsigned long StandardRequestLength(
#if NeedFunctionPrototypes
    xReq * /*req*/,
    ClientPtr /*client*/,
    int /*got*/,
    Bool * /*partp*/
#endif
);

extern int StandardReadRequestFromClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern int PendingClientOutput(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern int CheckPendingClientInput(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void MarkConnectionWriteBlocked(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern int BytesInClientBuffer(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void SkipInClientBuffer(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    int /*nbytes*/,
    int /*lenLastReq*/
#endif
);

extern Bool InsertFakeRequest(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    char * /*data*/,
    int /*count*/
#endif
);

extern void ResetCurrentRequest(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void FlushAllOutput(
#if NeedFunctionPrototypes
    void
#endif
);
extern int StandardWriteToClient(
#if NeedFunctionPrototypes
    ClientPtr /*who*/,
    int /*count*/,
    char * /*buf*/
#endif
);

extern int UncompressWriteToClient(
#if NeedFunctionPrototypes
    ClientPtr /*who*/,
    int /*count*/,
    char * /*buf*/
#endif
);

extern void ResetOsBuffers(
#if NeedFunctionPrototypes
    void
#endif
);

/* osinit.c */

extern void OsInit(
#if NeedFunctionPrototypes
    void
#endif
);

#endif
