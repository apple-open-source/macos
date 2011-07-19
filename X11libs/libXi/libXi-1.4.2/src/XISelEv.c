/************************************************************

Copyright 2009 Red Hat, Inc.

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
AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the author shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the author.

*/

/***********************************************************************
 *
 * XISelectEvent - Select for XI2 events.
 *
 */


#include <stdint.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XI2proto.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/extutil.h>
#include <X11/extensions/ge.h>
#include <X11/extensions/geproto.h>
#include "XIint.h"

int
XISelectEvents(Display* dpy, Window win, XIEventMask* masks, int num_masks)
{
    XIEventMask  *current;
    xXISelectEventsReq  *req;
    xXIEventMask mask;
    int i;
    int len = 0;
    int r = Success;

    XExtDisplayInfo *info = XInput_find_display(dpy);
    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_2_0, info) == -1) {
        r = NoSuchExtension;
        goto out;
    }
    GetReq(XISelectEvents, req);

    req->reqType = info->codes->major_opcode;
    req->ReqType = X_XISelectEvents;
    req->win = win;
    req->num_masks = num_masks;

    /* get the right length */
    for (i = 0; i < num_masks; i++)
    {
        len++;
        current = &masks[i];
        len += (current->mask_len + 3)/4;
    }

    SetReqLen(req, len, len);

    for (i = 0; i < num_masks; i++)
    {
        char *buff;
        current = &masks[i];
        mask.deviceid = current->deviceid;
        mask.mask_len = (current->mask_len + 3)/4;
        /* masks.mask_len is in bytes, but we need 4-byte units on the wire,
         * and they need to be padded with 0 */
        buff = calloc(1, mask.mask_len * 4);
        memcpy(buff, current->mask, current->mask_len);
        Data32(dpy, &mask, sizeof(xXIEventMask));
        Data(dpy, buff, mask.mask_len * 4);
        free(buff);
    }

out:
    UnlockDisplay(dpy);
    SyncHandle();
    return r;

}

XIEventMask*
XIGetSelectedEvents(Display* dpy, Window win, int *num_masks_return)
{
    int i, len = 0;
    unsigned char *mask;
    XIEventMask *mask_out = NULL;
    xXIEventMask *mask_in = NULL, *mi;
    xXIGetSelectedEventsReq *req;
    xXIGetSelectedEventsReply reply;
    XExtDisplayInfo *info = XInput_find_display(dpy);

    *num_masks_return = -1;
    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_2_0, info) == -1)
        goto out;

    GetReq(XIGetSelectedEvents, req);

    req->reqType = info->codes->major_opcode;
    req->ReqType = X_XIGetSelectedEvents;
    req->win = win;

    if (!_XReply(dpy, (xReply *) &reply, 0, xFalse))
        goto out;

    if (reply.num_masks == 0)
    {
        *num_masks_return = 0;
        goto out;
    }

    mask_in = Xmalloc(reply.length * 4);
    if (!mask_in)
        goto out;

    _XRead(dpy, (char*)mask_in, reply.length * 4);

    /* Memory layout of the XIEventMask for a 3 mask reply:
     * [struct a][struct b][struct c][masks a][masks b][masks c]
     */
    len = reply.num_masks * sizeof(XIEventMask);

    for (i = 0, mi = mask_in; i < reply.num_masks; i++)
    {
        len += mi->mask_len * 4;
        mi = (xXIEventMask*)((char*)mi + mi->mask_len * 4);
        mi++;
    }

    mask_out = Xmalloc(len);
    if (!mask_out)
        goto out;

    mi = mask_in;
    mask = (unsigned char*)&mask_out[reply.num_masks];
    for (i = 0; i < reply.num_masks; i++)
    {
        mask_out[i].deviceid = mi->deviceid;
        mask_out[i].mask_len = mi->mask_len * 4;
        mask_out[i].mask = mask;
        memcpy(mask_out[i].mask, &mi[1], mask_out[i].mask_len);
        mask += mask_out[i].mask_len;
        mi = (xXIEventMask*)((char*)mi + mi->mask_len * 4);
        mi++;
    }

    *num_masks_return = reply.num_masks;

out:
    Xfree(mask_in);

    UnlockDisplay(dpy);
    SyncHandle();

    return mask_out;
}
