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


int
get_button_map(Display	*display,
	       int	argc,
	       char	*argv[],
	       char	*name,
	       char	*desc)
{
    XDeviceInfo		*info;
    XDevice		*device;
    XAnyClassPtr	ip;
    int			i;
    int			nbuttons;

    if (argc != 1) {
	fprintf(stderr, "usage: xinput %s %s\n", name, desc);
	return EXIT_FAILURE;
    }

    info = find_device_info(display, argv[0], False);

    if (!info) {
	fprintf(stderr, "unable to find device %s\n", argv[0]);
	return EXIT_FAILURE;
    }

    ip = (XAnyClassPtr) info->inputclassinfo;
    nbuttons = 0;

    /* try to find the number of buttons */
    for(i=0; i<info->num_classes; i++) {
	if (ip->class == ButtonClass) {
	    nbuttons = ((XButtonInfoPtr)ip)->num_buttons;
	    break;
	}
	ip = (XAnyClassPtr) ((char *) ip + ip->length);
    }
    if (nbuttons == 0) {
	fprintf(stderr, "device has no buttons\n");
	return EXIT_FAILURE;
    }

    device = XOpenDevice(display, info->id);
    if (device) {
	int		idx;
	unsigned char	*map;

	map = (unsigned char *) malloc(sizeof(unsigned char) * nbuttons);

	XGetDeviceButtonMapping(display, device, map, nbuttons);

	for(idx=1; idx < nbuttons + 1; idx++) {
	    printf("%d ", map[idx - 1]);
	}
        printf("\n");
	XCloseDevice(display, device);
	return EXIT_SUCCESS;
    } else {
	fprintf(stderr, "Unable to open device\n");
	return EXIT_FAILURE;
    }
}



int
set_button_map(Display	*display,
	       int	argc,
	       char	*argv[],
	       char	*name,
	       char	*desc)
{
    XDeviceInfo		*info;
    XDevice		*device;
    XAnyClassPtr	ip;
    int			i;
    int			nbuttons;

    if (argc < 2) {
	fprintf(stderr, "usage: xinput %s %s\n", name, desc);
	return EXIT_FAILURE;
    }

    info = find_device_info(display, argv[0], False);

    if (!info) {
	fprintf(stderr, "unable to find device %s\n", argv[0]);
	return EXIT_FAILURE;
    }

    ip = (XAnyClassPtr) info->inputclassinfo;
    nbuttons = 0;

    /* try to find the number of buttons */
    for(i=0; i<info->num_classes; i++) {
	if (ip->class == ButtonClass) {
	    nbuttons = ((XButtonInfoPtr)ip)->num_buttons;
	    break;
	}
	ip = (XAnyClassPtr) ((char *) ip + ip->length);
    }
    if (nbuttons == 0) {
	fprintf(stderr, "device has no buttons\n");
	return EXIT_FAILURE;
    }

    device = XOpenDevice(display, info->id);
    if (device) {
	int		idx;
	unsigned char	*map;
	int		min;

	map = (unsigned char *) malloc(sizeof(unsigned char) * nbuttons);

	XGetDeviceButtonMapping(display, device, map, nbuttons);

	min = (argc > nbuttons + 1) ? nbuttons + 1 : argc;

	for(idx=1; idx < min; idx++) {
	    map[idx - 1] = atoi(argv[idx]);
	}
	XSetDeviceButtonMapping(display, device, map, nbuttons);
	XCloseDevice(display, device);
	return EXIT_SUCCESS;
    } else {
	fprintf(stderr, "Unable to open device\n");
	return EXIT_FAILURE;
    }
}

/* end of buttonmap.c */
