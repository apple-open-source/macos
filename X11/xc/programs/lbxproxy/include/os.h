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
/* $XFree86: xc/programs/lbxproxy/include/os.h,v 1.8 2003/11/17 22:20:48 dawes Exp $ */

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
    int * /*pClientsReady*/,
    Bool  /* poll */
);

/* connection.c */

extern Bool NewOutputPending;
extern Bool AnyClientsWriteBlocked;
extern int GrabInProgress;

extern void InitConnectionLimits(
    void
);

extern void CreateServerSockets(
    int * /*listen_fds[]*/
);

extern void CloseServerSockets(
    void
);

extern void CreateWellKnownSockets(
    void
);

extern void ListenToProxyManager (
    void
);

extern void ListenWellKnownSockets (
    void
);

extern void ResetWellKnownSockets(
    void
);

extern void AvailableClientInput(
    ClientPtr /*client*/
);

extern ClientPtr AllocNewConnection(
    int /*fd*/,			/* the fd to the display */
    int /*connect_fd*/,		/* the fd the client connected on */
    Bool /*to_server*/,
    XtransConnInfo /*trans_conn*/
);

extern void SwitchConnectionFuncs(
    ClientPtr /*client*/,
    int (* /*Read*/)(),
    int (* /*Writev*/)()
);

extern void StartOutputCompression(
    ClientPtr /*client*/,
    void (* /*CompressOn*/)(),
    void (* /*CompressOff*/)()
);

extern Bool EstablishNewConnections(
    ClientPtr /*clientUnused*/,
    pointer /*closure*/
);

extern void CloseDownFileDescriptor(
    ClientPtr /*client*/
);

extern void CheckConnections(
    void
);

extern void CloseDownConnection(
    ClientPtr /*client*/
);

extern void OnlyListenToOneClient(
    ClientPtr /*client*/
);

extern void ListenToAllClients(
    void
);

extern void IgnoreClient(
    ClientPtr /*client*/
);

extern void AttendClient(
    ClientPtr /*client*/
);

extern void MakeClientGrabImpervious(
    ClientPtr /*client*/
);

extern void MakeClientGrabPervious(
    ClientPtr /*client*/
);

/* io.c */

extern unsigned long StandardRequestLength(
    xReq * /*req*/,
    ClientPtr /*client*/,
    int /*got*/,
    Bool * /*partp*/
);

extern int StandardReadRequestFromClient(
    ClientPtr /*client*/
);

extern int PendingClientOutput(
    ClientPtr /*client*/
);

extern int CheckPendingClientInput(
    ClientPtr /*client*/
);

extern void MarkConnectionWriteBlocked(
    ClientPtr /*client*/
);

extern int BytesInClientBuffer(
    ClientPtr /*client*/
);

extern void SkipInClientBuffer(
    ClientPtr /*client*/,
    int /*nbytes*/,
    int /*lenLastReq*/
);

extern Bool InsertFakeRequest(
    ClientPtr /*client*/,
    char * /*data*/,
    int /*count*/
);

extern void ResetCurrentRequest(
    ClientPtr /*client*/
);

extern void FlushAllOutput(
    void
);
extern int StandardWriteToClient(
    ClientPtr /*who*/,
    int /*count*/,
    char * /*buf*/
);

extern int UncompressWriteToClient(
    ClientPtr /*who*/,
    int /*count*/,
    char * /*buf*/
);

extern void ResetOsBuffers(
    void
);

/* osinit.c */

extern void OsInit(
    void
);

#endif
