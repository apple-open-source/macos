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
#include <ctype.h>
#include <string.h>

int xi_opcode;

typedef int (*prog)(Display* display, int argc, char *argv[],
		    char *prog_name, char *prog_desc);

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
    {"get-button-map",
     "<device name>",
     get_button_map
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
     "[--short || --long] [<device name>...]",
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
#if HAVE_XI2
    { "create-master",
      "<id> [<sendCore (dflt:1)>] [<enable (dflt:1)>]",
      create_master
    },
    { "remove-master",
      "<id> [Floating|AttachToMaster (dflt:Floating)] [<returnPointer>] [<returnKeyboard>]",
      remove_master
    },
    { "reattach",
      "<id> <master>",
      change_attachment
    },
    { "float",
      "<id>",
      float_device
    },
    { "set-cp",
      "<window> <device>",
      set_clientpointer
    },
    { "test-xi2",
      "<device>",
      test_xi2,
    },
#endif
    { "list-props",
      "<device> [<device> ...]",
      list_props
    },
    { "set-int-prop",
      "<device> <property> <format (8, 16, 32)> <val> [<val> ...]",
      set_int_prop
    },
    { "set-float-prop",
      "<device> <property> <val> [<val> ...]",
      set_float_prop
    },
    { "set-atom-prop",
      "<device> <property> <val> [<val> ...]",
      set_atom_prop
    },
    { "watch-props",
      "<device>",
      watch_props
    },
    { "delete-prop",
      "<device> <property>",
      delete_prop
    },
    { "set-prop",
      "<device> [--type=atom|float|int] [--format=8|16|32] <property> <val> [<val> ...]",
      set_prop
    },
    {NULL, NULL, NULL
    }
};

static const char version_id[] = VERSION;

int
print_version()
{
    XExtensionVersion	*version;
    Display *display;

    printf("xinput version %s\n", version_id);

    display = XOpenDisplay(NULL);

    printf("XI version on server: ");

    if (display == NULL)
        printf("Failed to open display.\n");
    else {
        version = XGetExtensionVersion(display, INAME);
        if (!version || (version == (XExtensionVersion*) NoSuchExtension))
            printf(" Extension not supported.\n");
        else {
            printf("%d.%d\n", version->major_version,
                    version->minor_version);
            XFree(version);
            return 0;
        }
    }

    return 1;
}

int
xinput_version(Display	*display)
{
    XExtensionVersion	*version;
    static int vers = -1;

    if (vers != -1)
        return vers;

    version = XGetExtensionVersion(display, INAME);

    if (version && (version != (XExtensionVersion*) NoSuchExtension)) {
	vers = version->major_version;
	XFree(version);
    }

    return vers;
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
    XID		id = (XID)-1;

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
		return NULL;
	    } else {
		found = &devices[loop];
	    }
	}
    }
    return found;
}

#ifdef HAVE_XI2
Bool is_pointer(int use)
{
    return use == XIMasterPointer || use == XISlavePointer;
}

Bool is_keyboard(int use)
{
    return use == XIMasterKeyboard || use == XISlaveKeyboard;
}

Bool device_matches(XIDeviceInfo *info, char *name)
{
    if (strcmp(info->name, name) == 0) {
        return True;
    }

    if (strncmp(name, "pointer:", strlen("pointer:")) == 0 &&
        strcmp(info->name, name + strlen("pointer:")) == 0 &&
        is_pointer(info->use)) {
        return True;
    }

    if (strncmp(name, "keyboard:", strlen("keyboard:")) == 0 &&
        strcmp(info->name, name + strlen("keyboard:")) == 0 &&
        is_keyboard(info->use)) {
        return True;
    }

    return False;
}

XIDeviceInfo*
xi2_find_device_info(Display *display, char *name)
{
    XIDeviceInfo *info;
    XIDeviceInfo *found = NULL;
    int ndevices;
    Bool is_id = True;
    int i, id = -1;

    for(i = 0; i < strlen(name); i++) {
	if (!isdigit(name[i])) {
	    is_id = False;
	    break;
	}
    }

    if (is_id) {
	id = atoi(name);
    }

    info = XIQueryDevice(display, XIAllDevices, &ndevices);
    for(i = 0; i < ndevices; i++)
    {
        if (is_id ? info[i].deviceid == id : device_matches (&info[i], name)) {
            if (found) {
                fprintf(stderr,
                        "Warning: There are multiple devices matching '%s'.\n"
                        "To ensure the correct one is selected, please use "
                        "the device ID, or prefix the\ndevice name with "
                        "'pointer:' or 'keyboard:' as appropriate.\n\n", name);
                XIFreeDeviceInfo(info);
                return NULL;
            } else {
                found = &info[i];
            }
        }
    }

    return found;
}
#endif

static void
usage(void)
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
    int event, error;

    if (argc < 2) {
	usage();
	return EXIT_FAILURE;
    }

    func = argv[1];
    while((*func) == '-') func++;

    if (strcmp("version", func) == 0) {
        return print_version(argv[0]);
    }

    display = XOpenDisplay(NULL);

    if (display == NULL) {
	fprintf(stderr, "Unable to connect to X server\n");
	return EXIT_FAILURE;
    }

    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error)) {
        printf("X Input extension not available.\n");
        return EXIT_FAILURE;
    }

    if (!xinput_version(display)) {
	fprintf(stderr, "%s extension not available\n", INAME);
	return EXIT_FAILURE;
    }

    while(driver->func_name) {
	if (strcmp(driver->func_name, func) == 0) {
	    int	r = (*driver->func)(display, argc-2, argv+2,
				    driver->func_name, driver->arg_desc);
	    XSync(display, False);
	    XCloseDisplay(display);
	    return r;
	}
	driver++;
    }

    usage();

    return EXIT_FAILURE;
}

/* end of xinput.c */
