/* $Xorg: FSQXExt.c,v 1.4 2001/02/09 02:03:25 xorgcvs Exp $ */

/*
 * Copyright 1990 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software 
 * and its documentation for any purpose is hereby granted without fee, 
 * provided that the above copyright notice appear in all copies and 
 * that both that copyright notice and this permission notice appear 
 * in supporting documentation, and that the names of Network Computing 
 * Devices or Digital not be used in advertising or publicity pertaining 
 * to distribution of the software without specific, written prior 
 * permission. Network Computing Devices or Digital make no representations 
 * about the suitability of this software for any purpose.  It is provided 
 * "as is" without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES AND  DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES 
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, 
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS 
 * SOFTWARE.
 */
/* $XFree86: xc/lib/FS/FSQXExt.c,v 1.5 2001/12/14 19:53:33 dawes Exp $ */

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

*/

#include "FSlibint.h"

static void
_FS_convert_char_info(fsXCharInfo *src, FSXCharInfo *dst)
{
    dst->ascent = src->ascent;
    dst->descent = src->descent;
    dst->left = src->left;
    dst->right = src->right;
    dst->width = src->width;
    dst->attributes = src->attributes;
}

int
FSQueryXExtents8(svr, fid, range_type, str, str_len, extents)
    FSServer   *svr;
    Font        fid;
    Bool        range_type;
    unsigned char *str;
    unsigned long str_len;
    FSXCharInfo **extents;
{
    fsQueryXExtents8Req *req;
    fsQueryXExtents8Reply reply;
    FSXCharInfo *ext;
    fsXCharInfo local_exts;
    int         i;

    GetReq(QueryXExtents8, req);
    req->fid = fid;
    req->range = range_type;
    req->num_ranges = str_len;
    req->length += (str_len + 3) >> 2;
    _FSSend(svr, (char *) str, str_len);

    /* get back the info */
    if (!_FSReply(svr, (fsReply *) & reply,
	       (SIZEOF(fsQueryXExtents8Reply) - SIZEOF(fsGenericReply)) >> 2,
		  fsFalse))
	return FSBadAlloc;

    ext = (FSXCharInfo *) FSmalloc(sizeof(FSXCharInfo) * reply.num_extents);
    *extents = ext;
    if (!ext)
	return FSBadAlloc;
    for (i = 0; i < reply.num_extents; i++) {
	_FSReadPad(svr, (char *) &local_exts, SIZEOF(fsXCharInfo));
	_FS_convert_char_info(&local_exts, &ext[i]);
    }

    SyncHandle();
    return FSSuccess;
}

int
FSQueryXExtents16(svr, fid, range_type, str, str_len, extents)
    FSServer   *svr;
    Font        fid;
    Bool        range_type;
    FSChar2b   *str;
    unsigned long str_len;
    FSXCharInfo **extents;
{
    fsQueryXExtents16Req *req;
    fsQueryXExtents16Reply reply;
    FSXCharInfo *ext;
    fsXCharInfo local_exts;
    int         i;

    GetReq(QueryXExtents16, req);
    req->fid = fid;
    req->range = range_type;
    req->num_ranges = str_len;
    req->length += ((str_len * SIZEOF(fsChar2b)) + 3) >> 2;
    if (FSProtocolVersion(svr) == 1)
    {
	fsChar2b_version1 *swapped_str;

	swapped_str = (fsChar2b_version1 *)
	    FSmalloc(SIZEOF(fsChar2b_version1) * str_len);
	if (!swapped_str)
	    return FSBadAlloc;
	for (i = 0; i < str_len; i++) {
	    swapped_str[i].low = str[i].low;
	    swapped_str[i].high = str[i].high;
	}
	_FSSend(svr, (char *)swapped_str, (str_len*SIZEOF(fsChar2b_version1)));
	FSfree(swapped_str);
    } else
	_FSSend(svr, (char *) str, (str_len * SIZEOF(fsChar2b)));

    /* get back the info */
    if (!_FSReply(svr, (fsReply *) & reply,
	      (SIZEOF(fsQueryXExtents16Reply) - SIZEOF(fsGenericReply)) >> 2,
		  fsFalse))
	return FSBadAlloc;

    ext = (FSXCharInfo *) FSmalloc(sizeof(FSXCharInfo) * reply.num_extents);
    *extents = ext;
    if (!ext)
	return FSBadAlloc;
    for (i = 0; i < reply.num_extents; i++) {
	_FSReadPad(svr, (char *) &local_exts, SIZEOF(fsXCharInfo));
	_FS_convert_char_info(&local_exts, &ext[i]);
    }

    SyncHandle();
    return FSSuccess;
}
