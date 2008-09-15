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
#include <ctype.h>
#include <string.h>

typedef int (*prog)(
#if NeedFunctionPrototypes
		    Display* display, int argc, char *argv[],
		    char *prog_name, char *prog_desc
#endif
);

typedef struct 
{
    char	*func_name;
    char	*arg_desc;
    prog	func;
} entry;

static entry drivers[] =
{
    {"get-feedbacks",
     "<device name>",
     get_feedbacks
    },
    {"set-ptr-feedback",
     "<device name> <threshold> <num> <denom>",
     set_ptr_feedback
    },
    {"set-integer-feedback",
     "<device name> <feedback id> <value>",
     set_integer_feedback
    },
    {"set-button-map",
     "<device name> <map button 1> [<map button 2> [...]]",
     set_button_map
    },
    {"set-pointer",
     "<device name> [<x index> <y index>]",
     set_pointer
    },
    {"set-mode",
     "<device name> ABSOLUTE|RELATIVE",
     set_mode
    },
    {"list",
     "[--short || <device name>...]",
     list
    },
    {"query-state",
     "<device name>",
     query_state
    },
    {"test",
     "[-proximity] <device name>",
     test
    },
    {"version",
     "",
     version
    },
    {0, 0, 0
    }
};

static Bool
is_xinput_present(Display	*display)
{
    XExtensionVersion	*version;
    Bool		present;
    
    version = XGetExtensionVersion(display, INAME);

    if (version && (version != (XExtensionVersion*) NoSuchExtension)) {
	present = version->present;
	XFree(version);
	return present;
    } else {
	return False;
    }
}

XDeviceInfo*
find_device_info(Display	*display,
		 char		*name,
		 Bool		only_extended)
{
    XDeviceInfo	*devices;
    XDeviceInfo *found = NULL;
    int		loop;
    int		num_devices;
    int		len = strlen(name);
    Bool	is_id = True;
    XID		id;

    for(loop=0; loop<len; loop++) {
	if (!isdigit(name[loop])) {
	    is_id = False;
	    break;
	}
    }

    if (is_id) {
	id = atoi(name);
    }

    devices = XListInputDevices(display, &num_devices);

    for(loop=0; loop<num_devices; loop++) {
	if ((!only_extended || (devices[loop].use >= IsXExtensionDevice)) &&
	    ((!is_id && strcmp(devices[loop].name, name) == 0) ||
	     (is_id && devices[loop].id == id))) {
	    if (found) {
	        fprintf(stderr,
	                "Warning: There are multiple devices named \"%s\".\n"
	                "To ensure the correct one is selected, please use "
	                "the device ID instead.\n\n", name);
	    } else {
		found = &devices[loop];
	    }
	}
    }
    return found;
}

static void
usage()
{
    entry	*pdriver = drivers;

    fprintf(stderr, "usage :\n");

    while(pdriver->func_name) {
	fprintf(stderr, "\txinput %s %s\n", pdriver->func_name,
		pdriver->arg_desc);
	pdriver++;
    }
}

int
main(int argc, char * argv[])
{
    Display	*display;
    entry	*driver = drivers;
    char        *func;

    if (argc < 2) {
	usage();
	return EXIT_FAILURE;
    }

    display = XOpenDisplay(NULL);

    if (display == NULL) {
	fprintf(stderr, "Unable to connect to X server\n");
	return EXIT_FAILURE;
    }

    func = argv[1];
    while((*func) == '-') func++;

    if (!is_xinput_present(display)) {
	fprintf(stderr, "%s extension not available\n", INAME);
	return EXIT_FAILURE;
    }

    while(driver->func_name) {
	if (strcmp(driver->func_name, func) == 0) {
	    int	r = (*driver->func)(display, argc-2, argv+2,
				    driver->func_name, driver->arg_desc);
	    XFlush(display);
	    return r;
	}
	driver++;
    }

    usage();

    return EXIT_FAILURE;
}

/* end of xinput.c */
