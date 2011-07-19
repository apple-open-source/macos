/*
 * Copyright © 2009 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdint.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XI2proto.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/extutil.h>
#include "XIint.h"

extern int copy_classes(XIDeviceInfo* to, xXIAnyInfo* from, int nclasses);
extern int size_classes(xXIAnyInfo* from, int nclasses);

XIDeviceInfo*
XIQueryDevice(Display *dpy, int deviceid, int *ndevices_return)
{
    XIDeviceInfo        *info = NULL;
    xXIQueryDeviceReq   *req;
    xXIQueryDeviceReply reply;
    char                *ptr;
    int                 i;
    char                *buf;

    XExtDisplayInfo *extinfo = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_2_0, extinfo) == -1)
	goto error;

    GetReq(XIQueryDevice, req);
    req->reqType  = extinfo->codes->major_opcode;
    req->ReqType  = X_XIQueryDevice;
    req->deviceid = deviceid;

    if (!_XReply(dpy, (xReply*) &reply, 0, xFalse))
        goto error;

    *ndevices_return = reply.num_devices;
    info = Xmalloc((reply.num_devices + 1) * sizeof(XIDeviceInfo));
    if (!info)
        goto error;

    buf = Xmalloc(reply.length * 4);
    _XRead(dpy, buf, reply.length * 4);
    ptr = buf;

    /* info is a null-terminated array */
    info[reply.num_devices].name = NULL;

    for (i = 0; i < reply.num_devices; i++)
    {
        XIDeviceInfo    *lib = &info[i];
        xXIDeviceInfo   *wire = (xXIDeviceInfo*)ptr;

        lib->deviceid    = wire->deviceid;
        lib->use         = wire->use;
        lib->attachment  = wire->attachment;
        lib->enabled     = wire->enabled;
        lib->num_classes = wire->num_classes;
        lib->classes     = (XIAnyClassInfo**)&lib[1];

        ptr += sizeof(xXIDeviceInfo);

        lib->name = Xcalloc(wire->name_len + 1, 1);
        strncpy(lib->name, ptr, wire->name_len);
        ptr += ((wire->name_len + 3)/4) * 4;

        lib->classes = Xmalloc(size_classes((xXIAnyInfo*)ptr, lib->num_classes));
        ptr += copy_classes(lib, (xXIAnyInfo*)ptr, lib->num_classes);
    }

    Xfree(buf);
    UnlockDisplay(dpy);
    SyncHandle();
    return info;

error:
    UnlockDisplay(dpy);
    SyncHandle();
    *ndevices_return = -1;
    return NULL;
}

void
XIFreeDeviceInfo(XIDeviceInfo* info)
{
    XIDeviceInfo *ptr = info;
    while(ptr->name)
    {
        Xfree(ptr->classes);
        Xfree(ptr->name);
        ptr++;
    }
    Xfree(info);
}
