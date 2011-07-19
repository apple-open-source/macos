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
    XIAddMasterInfo c;

    if (argc == 0)
    {
        fprintf(stderr, "Usage: xinput %s %s\n", name, desc);
        return EXIT_FAILURE;
    }

    c.type = XIAddMaster;
    c.name = argv[0];
    c.send_core = (argc >= 2) ? atoi(argv[1]) : 1;
    c.enable = (argc >= 3) ? atoi(argv[2]) : 1;

    return XIChangeHierarchy(dpy, (XIAnyHierarchyChangeInfo*)&c, 1);
}

/**
 * Remove a master device.
 * By default, all attached devices are set to Floating, unless parameters are
 * given.
 */
int
remove_master(Display* dpy, int argc, char** argv, char *name, char *desc)
{
    XIRemoveMasterInfo r;
    XIDeviceInfo *info;
    int ret;

    if (argc == 0)
    {
        fprintf(stderr, "usage: xinput %s %s\n", name, desc);
        return EXIT_FAILURE;
    }

    info = xi2_find_device_info(dpy, argv[0]);

    if (!info) {
	fprintf(stderr, "unable to find device %s\n", argv[0]);
	return EXIT_FAILURE;
    }

    r.type = XIRemoveMaster;
    r.deviceid = info->deviceid;
    if (argc >= 2)
    {
        if (!strcmp(argv[1], "Floating"))
            r.return_mode = XIFloating;
        else if (!strcmp(argv[1], "AttachToMaster"))
            r.return_mode = XIAttachToMaster;
        else
            Error(BadValue, "Invalid return_mode.\n");
    } else
        r.return_mode = XIFloating;

    if (r.return_mode == XIAttachToMaster)
    {
        r.return_pointer = 0;
        if (argc >= 3) {
            info = xi2_find_device_info(dpy, argv[2]);
            if (!info) {
                fprintf(stderr, "unable to find device %s\n", argv[2]);
                return EXIT_FAILURE;
            }

            r.return_pointer = info->deviceid;
        }

        r.return_keyboard = 0;
        if (argc >= 4) {
            info = xi2_find_device_info(dpy, argv[3]);
            if (!info) {
                fprintf(stderr, "unable to find device %s\n", argv[3]);
                return EXIT_FAILURE;
            }

            r.return_keyboard = info->deviceid;
        }

        if (!r.return_pointer || !r.return_keyboard) {
            int i, ndevices;
            info = XIQueryDevice(dpy, XIAllMasterDevices, &ndevices);
            for(i = 0; i < ndevices; i++) {
                if (info[i].use == XIMasterPointer && !r.return_pointer)
                    r.return_pointer = info[i].deviceid;
                if (info[i].use == XIMasterKeyboard && !r.return_keyboard)
                    r.return_keyboard = info[i].deviceid;
                if (r.return_pointer && r.return_keyboard)
                    break;
            }

            XIFreeDeviceInfo(info);
        }
    }

    ret = XIChangeHierarchy(dpy, (XIAnyHierarchyChangeInfo*)&r, 1);
    return ret;
}

/**
 * Swap a device from one master to another.
 */
int
change_attachment(Display* dpy, int argc, char** argv, char *name, char* desc)
{
    XIDeviceInfo *sd_info, *md_info;
    XIAttachSlaveInfo c;
    int ret;

    if (argc < 2)
    {
        fprintf(stderr, "usage: xinput %s %s\n", name, desc);
        return EXIT_FAILURE;
    }

    sd_info = xi2_find_device_info(dpy, argv[0]);
    md_info= xi2_find_device_info(dpy, argv[1]);

    if (!sd_info) {
	fprintf(stderr, "unable to find device %s\n", argv[0]);
	return EXIT_FAILURE;
    }

    if (!md_info) {
	fprintf(stderr, "unable to find device %s\n", argv[1]);
	return EXIT_FAILURE;
    }

    c.type = XIAttachSlave;
    c.deviceid = sd_info->deviceid;
    c.new_master = md_info->deviceid;

    ret = XIChangeHierarchy(dpy, (XIAnyHierarchyChangeInfo*)&c, 1);
    return ret;
}

/**
 * Set a device floating.
 */
int
float_device(Display* dpy, int argc, char** argv, char* name, char* desc)
{
    XIDeviceInfo *info;
    XIDetachSlaveInfo c;
    int ret;

    if (argc < 1)
    {
        fprintf(stderr, "usage: xinput %s %s\n", name, desc);
        return EXIT_FAILURE;
    }

    info = xi2_find_device_info(dpy, argv[0]);

    if (!info) {
	fprintf(stderr, "unable to find device %s\n", argv[0]);
	return EXIT_FAILURE;
    }

    c.type = XIDetachSlave;
    c.deviceid = info->deviceid;

    ret = XIChangeHierarchy(dpy, (XIAnyHierarchyChangeInfo*)&c, 1);
    return ret;
}


