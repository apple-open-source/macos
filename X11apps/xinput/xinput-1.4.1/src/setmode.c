/*
 * Copyright 1996 by Frederic Lepied, France. <Frederic.Lepied@sugix.frmug.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  the authors  not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     The authors  make  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHORS DISCLAIM ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL THE AUTHORS  BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "xinput.h"
#include <string.h>

int
set_mode(Display	*display,
	 int		argc,
	 char		*argv[],
	 char		*name,
	 char		*desc)
{
    XDeviceInfo		*info;
    XDevice		*device;
    int			mode;

    if (argc != 2) {
	fprintf(stderr, "usage: xinput %s %s\n", name, desc);
	return EXIT_FAILURE;
    }

    info = find_device_info(display, argv[0], True);

    if (!info) {
	fprintf(stderr, "unable to find device %s\n", argv[0]);
	return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "ABSOLUTE") == 0) {
	mode = Absolute;
    } else {
	mode = Relative;
    }

    device = XOpenDevice(display, info->id);

    if (device) {
	XSetDeviceMode(display, device, mode);

	return EXIT_SUCCESS;
    } else {
	fprintf(stderr, "Unable to open device\n");
	return EXIT_FAILURE;
    }
}

/* end of setmode.c */
