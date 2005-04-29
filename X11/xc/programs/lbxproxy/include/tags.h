/* $Xorg: tags.h,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */
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
 * Copyright 1993 Network Computing Devices, Inc.
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
/* $XFree86: xc/programs/lbxproxy/include/tags.h,v 1.6 2003/11/17 22:20:49 dawes Exp $ */

#ifndef _TAGS_H_
#define _TAGS_H_

#include	"cache.h"

typedef struct _tagdata {
    XID         tid;
    int         data_type;
    unsigned long size;
    pointer     tdata;
}           TagDataRec;

typedef struct _tagdata *TagData;

extern void TagsInit(
    XServerPtr /*server*/,
    Bool /*useTags*/
);

extern void FreeTags(
    void
);

extern Bool TagStoreData(
    XServerPtr /*server*/,
    Cache /*cache*/,
    CacheID /*id*/,
    int /*size*/,
    int /*dtype*/,
    pointer /*data*/
);

extern Bool TagStoreDataNC(
    XServerPtr /*server*/,
    Cache /*cache*/,
    CacheID /*id*/,
    int /*size*/,
    int /*dtype*/,
    pointer /*data*/
);

extern TagData TagGetTag(
    XServerPtr /*server*/,
    Cache /*cache*/,
    CacheID /*id*/
);

extern pointer TagGetData(
    XServerPtr /*server*/,
    Cache /*cache*/,
    CacheID /*id*/
);

extern void TagFreeData(
    XServerPtr /*server*/,
    Cache /*cache*/,
    CacheID /*id*/,
    Bool /*notify*/
);

extern Bool AnyTagBearingReplies(
    XServerPtr /*server*/,
    Cache /*cache*/
);

extern Cache global_cache;
extern Cache prop_cache;

#endif				/* _TAGS_H_ */
