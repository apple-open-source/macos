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

#ifndef _LBXEXT_H_
#define _LBXEXT_H_

typedef int (*ExtensionRequests) (
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);
typedef int (*ExtensionReplies) (
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);
typedef int (*ExtensionEvents) (
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);
typedef int (*ExtensionErrors) (
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);
typedef int (*SExtensionRequests) (
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);
typedef int (*SExtensionReplies) (
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);
typedef int (*SExtensionEvents) (
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);
typedef int (*SExtensionErrors) (
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
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
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);
extern int  (*SwappedProcVector[256]) (
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

typedef struct _LbxQueryExtensionReply *xLbxQueryExtensionReplyPtr;

extern void DeleteExtensions(
#if NeedFunctionPrototypes
    XServerPtr /*server*/
#endif
);

typedef struct _replystuff *_ReplyStuffPtr;

extern void HandleExtensionError(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    xError * /*err*/,
    _ReplyStuffPtr /*nr*/
#endif
);

extern void HandleExtensionEvent(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    xEvent * /*ev*/
#endif
);

extern Bool HandleExtensionReply(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    xReply * /*reply*/,
    _ReplyStuffPtr * /*nr*/
#endif
);

extern Bool CheckExtensionForEvents(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    xReq * /*req*/
#endif
);

extern Bool CheckExtensionForReplies(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    xReq * /*req*/
#endif
);

extern int ProcLBXQueryExtension(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

#endif				/* _LBXEXT_H_ */
