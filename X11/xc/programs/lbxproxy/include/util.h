/* $Xorg: util.h,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */
/*

Copyright 1998  The Open Group

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
/*
 * Copyright 1994 Network Computing Devices, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name Network Computing Devices, Inc. not be
 * used in advertising or publicity pertaining to distribution of this 
 * software without specific, written prior permission.
 * 
 * THIS SOFTWARE IS PROVIDED `AS-IS'.  NETWORK COMPUTING DEVICES, INC.,
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT
 * LIMITATION ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, OR NONINFRINGEMENT.  IN NO EVENT SHALL NETWORK
 * COMPUTING DEVICES, INC., BE LIABLE FOR ANY DAMAGES WHATSOEVER, INCLUDING
 * SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS OF USE, DATA,
 * OR PROFITS, EVEN IF ADVISED OF THE POSSIBILITY THEREOF, AND REGARDLESS OF
 * WHETHER IN AN ACTION IN CONTRACT, TORT OR NEGLIGENCE, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 */
/* $XFree86: xc/programs/lbxproxy/include/util.h,v 1.8 2003/11/17 22:20:49 dawes Exp $ */

#ifndef	_UTIL_H_
#define	_UTIL_H_

#ifdef SIGNALRETURNSINT
#define SIGVAL int
#else
#define SIGVAL void
#endif

typedef SIGVAL (*OsSigHandlerPtr)(
    int /* sig */
);

extern OsSigHandlerPtr OsSignal(
    int /*sig*/,
    OsSigHandlerPtr /*handler*/
);

extern void AutoResetServer(
    int /*sig*/
);

extern void GiveUp(
    int /*sig*/
);

extern void Error(
    char * /*str*/
);

extern CARD32 GetTimeInMillis(
    void
);

extern int AdjustWaitForDelay(
    pointer /*waitTime*/,
    unsigned long /*newdelay*/
);

extern void UseMsg(
    void
);

extern void ProcessCommandLine(
    int /*argc*/,
    char * /*argv*/[]
);

#define xalloc(size) Xalloc((unsigned long)(size))
#define xcalloc(size) Xcalloc((unsigned long)(size))
#define xrealloc(ptr, size) Xrealloc((pointer)(ptr), (unsigned long)(size))
#define xfree(ptr) Xfree((pointer)(ptr))

extern unsigned long *Xalloc(
    unsigned long /*amount*/
);

extern unsigned long *Xcalloc(
    unsigned long /*amount*/
);

extern unsigned long *Xrealloc(
    pointer /*ptr*/,
    unsigned long /*amount*/
);

extern void Xfree(
    pointer /*ptr*/
);

extern void OsInitAllocator(
    void
);

extern void AuditF(
    const char * /*f*/,
    ...
);

extern void FatalError(
    const char * /*f*/,
    ...
);

extern void ErrorF(
    const char * /*f*/,
    ...
);

extern char *strnalloc(
    char * /*str*/,
    int /*len*/
);

typedef struct _WorkQueue	*WorkQueuePtr;

extern void ProcessWorkQueue(
    void
);

extern Bool QueueWorkProc(
    Bool (* /*function*/)(),
    ClientPtr /*client*/,
    pointer /*closure*/
);

extern Bool ClientSleep(
    ClientPtr /*client*/,
    Bool (* /*function*/)(),
    pointer /*closure*/
);

extern Bool ClientSignal(
    ClientPtr /*client*/
);

extern void ClientWakeup(
    ClientPtr /*client*/
);

extern Bool ClientIsAsleep(
    ClientPtr /*client*/
);

extern void LBXReadAtomsFile(
    XServerPtr /*server*/
);

#endif				/* _UTIL_H_ */
