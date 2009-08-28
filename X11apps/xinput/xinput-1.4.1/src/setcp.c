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

int
set_clientpointer(Display* dpy, int argc, char** argv, char* name, char *desc)
{
    XDeviceInfo* info;
    XID window;
    XDevice* dev = NULL;
    char* id;
    char* dummy;

    if (argc <= 1)
    {
        fprintf(stderr, "Usage: xinput %s %s\n", name, desc);
        return EXIT_FAILURE;
    }

    id = argv[0];

    while(*id == '0') id++;

    window = strtol(argv[0], &dummy, (*id == 'x') ? 16 : 10);

    info = find_device_info(dpy, argv[1], False);

    if (!info) {
	fprintf(stderr, "unable to find device %s\n", argv[1]);
	return EXIT_FAILURE;
    }

    dev = XOpenDevice(dpy, info->id);

    if (!dev)
    {
        fprintf(stderr, "Cannot open device %s.\n", argv[1]);
    } else
        XSetClientPointer(dpy, window, dev);
    return 0;
}
