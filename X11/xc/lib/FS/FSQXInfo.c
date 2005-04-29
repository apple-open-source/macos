/* $Xorg: FSQXInfo.c,v 1.4 2001/02/09 02:03:25 xorgcvs Exp $ */
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
/* $XFree86: xc/lib/FS/FSQXInfo.c,v 1.6 2003/12/22 17:48:02 tsi Exp $ */

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

#include	"FSlibint.h"

/*
 * Note:  only the range in the first FSQuery is sent to the server.
 * the others exist as return values only.
 */

int
FSQueryXInfo(svr, fid, info, props, offsets, prop_data)
    FSServer   *svr;
    Font        fid;
    FSXFontInfoHeader *info;
    FSPropInfo *props;
    FSPropOffset **offsets;
    unsigned char **prop_data;
{
    fsQueryXInfoReq *req;
    fsQueryXInfoReply reply;
    FSPropOffset *offset_data;
    unsigned char *pdata;
    fsPropInfo local_pi;
    fsPropOffset local_po;
    int j;

    GetReq(QueryXInfo, req);
    req->id = fid;

    /* get back the info */
    if (!_FSReply(svr, (fsReply *) & reply, ((SIZEOF(fsQueryXInfoReply) -
			    SIZEOF(fsGenericReply)) >> 2), fsFalse)) {
	return FSBadAlloc;
    }

    FSUnpack_XFontInfoHeader(&reply, info, FSProtocolVersion(svr));

    /* get the prop header */
    _FSReadPad(svr, (char *) &local_pi, SIZEOF(fsPropInfo));
    props->num_offsets = local_pi.num_offsets;
    props->data_len = local_pi.data_len;

#if SIZE_MAX <= UINT_MAX
    if (props->num_offsets > SIZE_MAX / sizeof(FSPropOffset)) 
	return FSBadAlloc;
#endif

    /* prepare for prop data */
    offset_data = (FSPropOffset *)
	FSmalloc(props->num_offsets * sizeof(FSPropOffset));
    if (!offset_data)
	return FSBadAlloc;
    pdata = (unsigned char *) FSmalloc(props->data_len);
    if (!pdata) {
	FSfree((char *) offset_data);
	return FSBadAlloc;
    }
    /* get offsets */
    for (j=0; j<props->num_offsets; j++)
    {
	_FSReadPad(svr, (char *) &local_po, SIZEOF(fsPropOffset));
	offset_data[j].name.position = local_po.name.position;
	offset_data[j].name.length = local_po.name.length;
	offset_data[j].value.position = local_po.value.position;
	offset_data[j].value.length = local_po.value.length;
	offset_data[j].type = local_po.type;
    }

    /* get data */
    _FSReadPad(svr, (char *) pdata, props->data_len);
    *offsets = offset_data;
    *prop_data = pdata;

    SyncHandle();
    return FSSuccess;
}
