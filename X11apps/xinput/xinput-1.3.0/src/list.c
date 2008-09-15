/*
 * Copyright 1996 by Frederic Lepied, France. <Frederic.Lepied@sugix.frmug.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Frederic   Lepied not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Frederic  Lepied   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * FREDERIC  LEPIED DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL FREDERIC  LEPIED BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "xinput.h"
#include <string.h>

static void
print_info(XDeviceInfo	*info, Bool shortformat)
{
    int			i,j;
    XAnyClassPtr	any;
    XKeyInfoPtr		k;
    XButtonInfoPtr	b;
    XValuatorInfoPtr	v;
    XAxisInfoPtr	a;

    printf("\"%s\"\tid=%ld\t[", info->name, info->id);

    switch (info->use) {
    case IsXPointer:
       printf("XPointer");
       break;
    case IsXKeyboard:
       printf("XKeyboard");
       break;
    case IsXExtensionDevice:
       printf("XExtensionDevice");
       break;
    case IsXExtensionKeyboard:
       printf("XExtensionKeyboard");
       break;
    case IsXExtensionPointer:
       printf("XExtensionPointer");
       break;
    default:
       printf("Unknown class");
       break;
    }
    printf("]\n");

    if (shortformat)
        return;

    if (info->num_classes > 0) {
	any = (XAnyClassPtr) (info->inputclassinfo);
	for (i=0; i<info->num_classes; i++) {
	    switch (any->class) {
	    case KeyClass:
		k = (XKeyInfoPtr) any;
		printf("\tNum_keys is %d\n", k->num_keys);
		printf("\tMin_keycode is %d\n", k->min_keycode);
		printf("\tMax_keycode is %d\n", k->max_keycode);
		break;

	    case ButtonClass:
		b = (XButtonInfoPtr) any;
		printf("\tNum_buttons is %d\n", b->num_buttons);
		break;

	    case ValuatorClass:
		v = (XValuatorInfoPtr) any;
		a = (XAxisInfoPtr) ((char *) v +
				    sizeof (XValuatorInfo));
		printf("\tNum_axes is %d\n", v->num_axes);
		printf("\tMode is %s\n", (v->mode == Absolute) ? "Absolute" : "Relative");
		printf("\tMotion_buffer is %ld\n", v->motion_buffer);
		for (j=0; j<v->num_axes; j++, a++) {
		    printf("\tAxis %d :\n", j);
		    printf("\t\tMin_value is %d\n", a->min_value);
		    printf("\t\tMax_value is %d\n", a->max_value);
		    printf ("\t\tResolution is %d\n", a->resolution);
		}
		break;

	    default:
		printf ("unknown class\n");
	    }
	    any = (XAnyClassPtr) ((char *) any + any->length);
	}
    }
}

int
list(Display	*display,
     int	argc,
     char	*argv[],
     char	*name,
     char	*desc)
{
    XDeviceInfo		*info;
    int			loop;
    int                 shortformat = False;

    shortformat = (argc == 1 && strcmp(argv[0], "--short") == 0);

    if (argc == 0 || shortformat) {
	int		num_devices;

	info = XListInputDevices(display, &num_devices);

	for(loop=0; loop<num_devices; loop++) {
	    print_info(info+loop, shortformat);
	}
    } else {
	int	ret = EXIT_SUCCESS;

	for(loop=0; loop<argc; loop++) {
	    info = find_device_info(display, argv[0], False);

	    if (!info) {
		fprintf(stderr, "unable to find device %s\n", argv[0]);
		ret = EXIT_FAILURE;
	    } else {
		print_info(info, shortformat);
	    }
	}
	return ret;
    }
    return EXIT_SUCCESS;
}

/* end of list.c */
