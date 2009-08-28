/*
 * Copyright 2007 Peter Hutterer <peter@cs.unisa.edu.au>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the author shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization
 * from the author.
 *
 */

#include "xinput.h"
#include <string.h>

#define Error(error, ...) \
{ \
    fprintf(stderr, __VA_ARGS__); \
    return error;\
}
/**
 * Create a new master device. Name must be supplied, other values are
 * optional.
 */
int
create_master(Display* dpy, int argc, char** argv, char* name, char *desc)
{
    XCreateMasterInfo c;

    if (argc == 0)
    {
        fprintf(stderr, "Usage: xinput %s %s\n", name, desc);
        return EXIT_FAILURE;
    }

    c.type = CH_CreateMasterDevice;
    c.name = argv[0];
    c.sendCore = (argc >= 2) ? atoi(argv[1]) : 1;
    c.enable = (argc >= 3) ? atoi(argv[2]) : 1;

    return XChangeDeviceHierarchy(dpy, (XAnyHierarchyChangeInfo*)&c, 1);
}

/**
 * Remove a master device.
 * By default, all attached devices are set to Floating, unless parameters are
 * given.
 */
int
remove_master(Display* dpy, int argc, char** argv, char *name, char *desc)
{
    XDeviceInfo *info;
    XRemoveMasterInfo r;
    XDevice* master = NULL, *ptr = NULL, *keybd = NULL;
    int ret;

    if (argc == 0)
    {
        fprintf(stderr, "usage: xinput %s %s\n", name, desc);
        return EXIT_FAILURE;
    }

    info = find_device_info(dpy, argv[0], False);

    if (!info) {
	fprintf(stderr, "unable to find device %s\n", argv[0]);
	return EXIT_FAILURE;
    }

    master = XOpenDevice(dpy, info->id);
    if (!master)
        Error(BadValue, "Unable to open device %s.\n", argv[0]);

    r.type = CH_RemoveMasterDevice;
    r.device = master;
    if (argc >= 2)
    {
        if (!strcmp(argv[1], "Floating"))
            r.returnMode = Floating;
        else if (!strcmp(argv[1], "AttachToMaster"))
            r.returnMode = AttachToMaster;
        else
            Error(BadValue, "Invalid returnMode.\n");
    } else
        r.returnMode = Floating;

    if (r.returnMode == AttachToMaster)
    {
        ptr = XOpenDevice(dpy, atoi(argv[2]));
        keybd = XOpenDevice(dpy, atoi(argv[3]));
        if (!ptr || !keybd)
            Error(BadValue, "Invalid fallback master.\n");
        r.returnPointer = ptr;
        r.returnKeyboard = keybd;
    }

    ret = XChangeDeviceHierarchy(dpy, (XAnyHierarchyChangeInfo*)&r, 1);
    if (ptr)
        XCloseDevice(dpy, ptr);
    if (keybd)
        XCloseDevice(dpy, keybd);
    return ret;
}

/**
 * Swap a device from one master to another.
 */
int
change_attachment(Display* dpy, int argc, char** argv, char *name, char* desc)
{
    XDeviceInfo *info_sd, *info_md;
    XChangeAttachmentInfo c;
    XDevice *slave, *master;
    int ret;

    if (argc < 2)
    {
        fprintf(stderr, "usage: xinput %s %s\n", name, desc);
        return EXIT_FAILURE;
    }

    info_sd = find_device_info(dpy, argv[0], True);
    info_md = find_device_info(dpy, argv[1], False);

    if (!info_sd) {
	fprintf(stderr, "unable to find device %s\n", argv[0]);
	return EXIT_FAILURE;
    }

    if (!info_md) {
	fprintf(stderr, "unable to find device %s\n", argv[1]);
	return EXIT_FAILURE;
    }

    slave = XOpenDevice(dpy, info_sd->id);
    master = XOpenDevice(dpy, info_md->id);

    if (!slave)
        Error(BadValue, "Invalid slave device given %d\n", atoi(argv[0]));

    if (!master)
        Error(BadValue, "Invalid master device given %d\n", atoi(argv[1]));

    c.type = CH_ChangeAttachment;
    c.changeMode = AttachToMaster;
    c.device = slave;
    c.newMaster = master;

    ret = XChangeDeviceHierarchy(dpy, (XAnyHierarchyChangeInfo*)&c, 1);
    XCloseDevice(dpy, slave);
    XCloseDevice(dpy, master);
    return ret;
}

/**
 * Set a device floating.
 */
int
float_device(Display* dpy, int argc, char** argv, char* name, char* desc)
{
    XDeviceInfo *info;
    XChangeAttachmentInfo c;
    XDevice *slave;
    int ret;

    if (argc < 1)
    {
        fprintf(stderr, "usage: xinput %s %s\n", name, desc);
        return EXIT_FAILURE;
    }

    info = find_device_info(dpy, argv[0], True);

    if (!info) {
	fprintf(stderr, "unable to find device %s\n", argv[0]);
	return EXIT_FAILURE;
    }

    slave = XOpenDevice(dpy, info->id);

    if (!slave)
        return BadValue;

    c.type = CH_ChangeAttachment;
    c.changeMode = Floating;
    c.device = slave;

    ret = XChangeDeviceHierarchy(dpy, (XAnyHierarchyChangeInfo*)&c, 1);
    XCloseDevice(dpy, slave);
    return ret;
}


