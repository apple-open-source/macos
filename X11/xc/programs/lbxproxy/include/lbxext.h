/* $Xorg: lbxext.h,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */
/*

Copyright 1996, 1998  The Open Group

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
/* $XFree86: xc/programs/lbxproxy/include/lbxext.h,v 1.3 2003/11/17 22:20:48 dawes Exp $ */

#ifndef _LBXEXT_H_
#define _LBXEXT_H_

typedef int (*ExtensionRequests) (
    ClientPtr /*client*/
);
typedef int (*ExtensionReplies) (
    ClientPtr /*client*/
);
typedef int (*ExtensionEvents) (
    ClientPtr /*client*/
);
typedef int (*ExtensionErrors) (
    ClientPtr /*client*/
);
typedef int (*SExtensionRequests) (
    ClientPtr /*client*/
);
typedef int (*SExtensionReplies) (
    ClientPtr /*client*/
);
typedef int (*SExtensionEvents) (
    ClientPtr /*client*/
);
typedef int (*SExtensionErrors) (
    ClientPtr /*client*/
);

typedef struct _vectors {
    char       *name;
    ExtensionRequests req_vector;
    ExtensionReplies rep_vector;
    ExtensionEvents ev_vector;
    ExtensionErrors err_vector;
    SExtensionRequests sreq_vector;
}           ExtensionVectors;

extern int  (*ProcVector[256]) (
    ClientPtr /*client*/
);
extern int  (*SwappedProcVector[256]) (
    ClientPtr /*client*/
);

typedef struct _LbxQueryExtensionReply *xLbxQueryExtensionReplyPtr;

extern void DeleteExtensions(
    XServerPtr /*server*/
);

typedef struct _replystuff *_ReplyStuffPtr;

extern void HandleExtensionError(
    ClientPtr /*client*/,
    xError * /*err*/,
    _ReplyStuffPtr /*nr*/
);

extern void HandleExtensionEvent(
    ClientPtr /*client*/,
    xEvent * /*ev*/
);

extern Bool HandleExtensionReply(
    ClientPtr /*client*/,
    xReply * /*reply*/,
    _ReplyStuffPtr * /*nr*/
);

extern Bool CheckExtensionForEvents(
    ClientPtr /*client*/,
    xReq * /*req*/
);

extern Bool CheckExtensionForReplies(
    ClientPtr /*client*/,
    xReq * /*req*/
);

extern int ProcLBXQueryExtension(
    ClientPtr /*client*/
);

#endif				/* _LBXEXT_H_ */
