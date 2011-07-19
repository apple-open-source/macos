/*
 * Copyright 2007 Peter Hutterer
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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <X11/Xatom.h>
#include <X11/extensions/XIproto.h>

#include "xinput.h"

static Atom parse_atom(Display *dpy, char *name) {
    Bool is_atom = True;
    int i;

    for (i = 0; name[i] != '\0'; i++) {
        if (!isdigit(name[i])) {
            is_atom = False;
            break;
        }
    }

    if (is_atom)
        return atoi(name);
    else
        return XInternAtom(dpy, name, False);
}

static void
print_property(Display *dpy, XDevice* dev, Atom property)
{
    Atom                act_type;
    char                *name;
    int                 act_format;
    unsigned long       nitems, bytes_after;
    unsigned char       *data, *ptr;
    int                 j, done = False, size;

    name = XGetAtomName(dpy, property);
    printf("\t%s (%ld):\t", name, property);

    if (XGetDeviceProperty(dpy, dev, property, 0, 1000, False,
                           AnyPropertyType, &act_type, &act_format,
                           &nitems, &bytes_after, &data) == Success)
    {
        Atom float_atom = XInternAtom(dpy, "FLOAT", True);

        ptr = data;

        if (nitems == 0)
            printf("<no items>");

        switch(act_format)
        {
            case 8: size = sizeof(char); break;
            case 16: size = sizeof(short); break;
            case 32: size = sizeof(long); break;
        }

        for (j = 0; j < nitems; j++)
        {
            switch(act_type)
            {
                case XA_INTEGER:
                    switch(act_format)
                    {
                        case 8:
                            printf("%d", *((char*)ptr));
                            break;
                        case 16:
                            printf("%d", *((short*)ptr));
                            break;
                        case 32:
                            printf("%ld", *((long*)ptr));
                            break;
                    }
                    break;
                case XA_STRING:
                    if (act_format != 8)
                    {
                        printf("Unknown string format.\n");
                        done = True;
                        break;
                    }
                    printf("\"%s\"", ptr);
                    j += strlen((char*)ptr); /* The loop's j++ jumps over the
                                                terminating 0 */
                    ptr += strlen((char*)ptr); /* ptr += size below jumps over
                                                  the terminating 0 */
                    break;
                case XA_ATOM:
                    {
                        Atom a = *(Atom*)ptr;
                        printf("\"%s\" (%d)",
                                (a) ? XGetAtomName(dpy, a) : "None",
                                (int)a);
                        break;
                    }
                default:
                    if (float_atom != None && act_type == float_atom)
                    {
                        printf("%f", *((float*)ptr));
                        break;
                    }

                    printf("\t... of unknown type %s\n",
                            XGetAtomName(dpy, act_type));
                    done = True;
                    break;
            }

            ptr += size;

            if (done == True)
                break;
            if (j < nitems - 1)
                printf(", ");
        }
        printf("\n");
        XFree(data);
    } else
        printf("\tFetch failure\n");

}

static int
list_props_xi1(Display *dpy, int argc, char** argv, char* name, char *desc)
{
    XDeviceInfo *info;
    XDevice     *dev;
    int          i;
    int         nprops;
    Atom        *props;

    if (argc == 0)
    {
        fprintf(stderr, "Usage: xinput %s %s\n", name, desc);
        return EXIT_FAILURE;
    }

    for (i = 0; i < argc; i++)
    {
        info = find_device_info(dpy, argv[i], False);
        if (!info)
        {
            fprintf(stderr, "unable to find device %s\n", argv[i]);
            continue;
        }

        dev = XOpenDevice(dpy, info->id);
        if (!dev)
        {
            fprintf(stderr, "unable to open device '%s'\n", info->name);
            continue;
        }

        props = XListDeviceProperties(dpy, dev, &nprops);
        if (!nprops)
        {
            printf("Device '%s' does not report any properties.\n", info->name);
            continue;
        }

        printf("Device '%s':\n", info->name);
        while(nprops--)
        {
            print_property(dpy, dev, props[nprops]);
        }

        XFree(props);
        XCloseDevice(dpy, dev);
    }
    return EXIT_SUCCESS;
}


int watch_props(Display *dpy, int argc, char** argv, char* n, char *desc)
{
    XDevice     *dev;
    XDeviceInfo *info;
    XEvent      ev;
    XDevicePropertyNotifyEvent *dpev;
    char        *name;
    int         type_prop;
    XEventClass cls_prop;

    if (list_props(dpy, argc, argv, n, desc) != EXIT_SUCCESS)
        return EXIT_FAILURE;

    info = find_device_info(dpy, argv[0], False);
    if (!info)
    {
        fprintf(stderr, "unable to find device %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    dev = XOpenDevice(dpy, info->id);
    if (!dev)
    {
        fprintf(stderr, "unable to open device '%s'\n", info->name);
        return EXIT_FAILURE;
    }

    DevicePropertyNotify(dev, type_prop, cls_prop);
    XSelectExtensionEvent(dpy, DefaultRootWindow(dpy), &cls_prop, 1);

    while(1)
    {
        XNextEvent(dpy, &ev);

        dpev = (XDevicePropertyNotifyEvent*)&ev;
        if (dpev->type != type_prop)
            continue;

        name = XGetAtomName(dpy, dpev->atom);
        printf("Property '%s' changed.\n", name);
        print_property(dpy, dev, dpev->atom);
    }

    XCloseDevice(dpy, dev);
}

static int
delete_prop_xi1(Display *dpy, int argc, char** argv, char* n, char *desc)
{
    XDevice     *dev;
    XDeviceInfo *info;
    char        *name;
    Atom        prop;

    info = find_device_info(dpy, argv[0], False);
    if (!info)
    {
        fprintf(stderr, "unable to find device %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    dev = XOpenDevice(dpy, info->id);
    if (!dev)
    {
        fprintf(stderr, "unable to open device '%s'\n", info->name);
        return EXIT_FAILURE;
    }

    name = argv[1];

    prop = parse_atom(dpy, name);

    XDeleteDeviceProperty(dpy, dev, prop);

    XCloseDevice(dpy, dev);
    return EXIT_SUCCESS;
}

static int
do_set_prop_xi1(Display *dpy, Atom type, int format, int argc, char **argv, char *n, char *desc)
{
    XDeviceInfo  *info;
    XDevice      *dev;
    Atom          prop;
    Atom          old_type;
    char         *name;
    int           i;
    Atom          float_atom;
    int           old_format, nelements = 0;
    unsigned long act_nitems, bytes_after;
    char         *endptr;
    union {
        unsigned char *c;
        short *s;
        long *l;
        Atom *a;
    } data;

    if (argc < 3)
    {
        fprintf(stderr, "Usage: xinput %s %s\n", n, desc);
        return EXIT_FAILURE;
    }

    info = find_device_info(dpy, argv[0], False);
    if (!info)
    {
        fprintf(stderr, "unable to find device %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    dev = XOpenDevice(dpy, info->id);
    if (!dev)
    {
        fprintf(stderr, "unable to open device %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    name = argv[1];

    prop = parse_atom(dpy, name);

    if (prop == None) {
        fprintf(stderr, "invalid property %s\n", name);
        return EXIT_FAILURE;
    }

    float_atom = XInternAtom(dpy, "FLOAT", False);

    nelements = argc - 2;
    if (type == None || format == 0) {
        if (XGetDeviceProperty(dpy, dev, prop, 0, 0, False, AnyPropertyType,
                               &old_type, &old_format, &act_nitems,
                               &bytes_after, &data.c) != Success) {
            fprintf(stderr, "failed to get property type and format for %s\n",
                    name);
            return EXIT_FAILURE;
        } else {
            if (type == None)
                type = old_type;
            if (format == 0)
                format = old_format;
        }

        XFree(data.c);
    }

    if (type == None) {
        fprintf(stderr, "property %s doesn't exist, you need to specify "
                "its type and format\n", name);
        return EXIT_FAILURE;
    }

    data.c = calloc(nelements, sizeof(long));

    for (i = 0; i < nelements; i++)
    {
        if (type == XA_INTEGER) {
            switch (format)
            {
                case 8:
                    data.c[i] = atoi(argv[2 + i]);
                    break;
                case 16:
                    data.s[i] = atoi(argv[2 + i]);
                    break;
                case 32:
                    data.l[i] = atoi(argv[2 + i]);
                    break;
                default:
                    fprintf(stderr, "unexpected size for property %s", name);
                    return EXIT_FAILURE;
            }
        } else if (type == float_atom) {
            if (format != 32) {
                fprintf(stderr, "unexpected format %d for property %s\n",
                        format, name);
                return EXIT_FAILURE;
            }
            *(float *)(data.l + i) = strtod(argv[2 + i], &endptr);
            if (endptr == argv[2 + i]) {
                fprintf(stderr, "argument %s could not be parsed\n", argv[2 + i]);
                return EXIT_FAILURE;
            }
        } else if (type == XA_ATOM) {
            if (format != 32) {
                fprintf(stderr, "unexpected format %d for property %s\n",
                        format, name);
                return EXIT_FAILURE;
            }
            data.a[i] = parse_atom(dpy, argv[2 + i]);
        } else {
            fprintf(stderr, "unexpected type for property %s\n", name);
            return EXIT_FAILURE;
        }
    }

    XChangeDeviceProperty(dpy, dev, prop, type, format, PropModeReplace,
                          data.c, nelements);
    free(data.c);
    XCloseDevice(dpy, dev);
    return EXIT_SUCCESS;
}

#if HAVE_XI2
static void
print_property_xi2(Display *dpy, int deviceid, Atom property)
{
    Atom                act_type;
    char                *name;
    int                 act_format;
    unsigned long       nitems, bytes_after;
    unsigned char       *data, *ptr;
    int                 j, done = False;

    name = XGetAtomName(dpy, property);
    printf("\t%s (%ld):\t", name, property);

    if (XIGetProperty(dpy, deviceid, property, 0, 1000, False,
                           AnyPropertyType, &act_type, &act_format,
                           &nitems, &bytes_after, &data) == Success)
    {
        Atom float_atom = XInternAtom(dpy, "FLOAT", True);

        ptr = data;

        if (nitems == 0)
            printf("<no items>");

        for (j = 0; j < nitems; j++)
        {
            switch(act_type)
            {
                case XA_INTEGER:
                    switch(act_format)
                    {
                        case 8:
                            printf("%d", *((int8_t*)ptr));
                            break;
                        case 16:
                            printf("%d", *((int16_t*)ptr));
                            break;
                        case 32:
                            printf("%d", *((int32_t*)ptr));
                            break;
                    }
                    break;
                case XA_STRING:
                    if (act_format != 8)
                    {
                        printf("Unknown string format.\n");
                        done = True;
                        break;
                    }
                    printf("\"%s\"", ptr);
                    j += strlen((char*)ptr); /* The loop's j++ jumps over the
                                                terminating 0 */
                    ptr += strlen((char*)ptr); /* ptr += size below jumps over
                                                  the terminating 0 */
                    break;
                case XA_ATOM:
                    {
                        Atom a = *(uint32_t*)ptr;
                        printf("\"%s\" (%ld)",
                                (a) ? XGetAtomName(dpy, a) : "None",
                                a);
                        break;
                    }
                    break;
                default:
                    if (float_atom != None && act_type == float_atom)
                    {
                        printf("%f", *((float*)ptr));
                        break;
                    }

                    printf("\t... of unknown type %s\n",
                            XGetAtomName(dpy, act_type));
                    done = True;
                    break;
            }

            ptr += act_format/8;

            if (done == True)
                break;
            if (j < nitems - 1)
                printf(", ");
        }
        printf("\n");
        XFree(data);
    } else
        printf("\tFetch failure\n");

}

static int
list_props_xi2(Display *dpy, int argc, char** argv, char* name, char *desc)
{
    XIDeviceInfo *info;
    int         i;
    int         nprops;
    Atom        *props;

    if (argc == 0)
    {
        fprintf(stderr, "Usage: xinput %s %s\n", name, desc);
        return EXIT_FAILURE;
    }

    for (i = 0; i < argc; i++)
    {
        info = xi2_find_device_info(dpy, argv[i]);
        if (!info)
        {
            fprintf(stderr, "unable to find device %s\n", argv[i]);
            continue;
        }

        props = XIListProperties(dpy, info->deviceid, &nprops);
        if (!nprops)
        {
            printf("Device '%s' does not report any properties.\n", info->name);
            continue;
        }

        printf("Device '%s':\n", info->name);
        while(nprops--)
        {
            print_property_xi2(dpy, info->deviceid, props[nprops]);
        }

        XFree(props);
    }
    return EXIT_SUCCESS;
}

static int
delete_prop_xi2(Display *dpy, int argc, char** argv, char* n, char *desc)
{
    XIDeviceInfo *info;
    char        *name;
    Atom        prop;

    info = xi2_find_device_info(dpy, argv[0]);
    if (!info)
    {
        fprintf(stderr, "unable to find device %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    name = argv[1];

    prop = parse_atom(dpy, name);

    XIDeleteProperty(dpy, info->deviceid, prop);

    return EXIT_SUCCESS;
}

static int
do_set_prop_xi2(Display *dpy, Atom type, int format, int argc, char **argv, char *n, char *desc)
{
    XIDeviceInfo *info;
    Atom          prop;
    Atom          old_type;
    char         *name;
    int           i;
    Atom          float_atom;
    int           old_format, nelements = 0;
    unsigned long act_nitems, bytes_after;
    char         *endptr;
    union {
        unsigned char *c;
        int16_t *s;
        int32_t *l;
    } data;

    if (argc < 3)
    {
        fprintf(stderr, "Usage: xinput %s %s\n", n, desc);
        return EXIT_FAILURE;
    }

    info = xi2_find_device_info(dpy, argv[0]);
    if (!info)
    {
        fprintf(stderr, "unable to find device %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    name = argv[1];

    prop = parse_atom(dpy, name);

    if (prop == None) {
        fprintf(stderr, "invalid property %s\n", name);
        return EXIT_FAILURE;
    }

    float_atom = XInternAtom(dpy, "FLOAT", False);

    nelements = argc - 2;
    if (type == None || format == 0) {
        if (XIGetProperty(dpy, info->deviceid, prop, 0, 0, False,
                          AnyPropertyType, &old_type, &old_format, &act_nitems,
                          &bytes_after, &data.c) != Success) {
            fprintf(stderr, "failed to get property type and format for %s\n",
                    name);
            return EXIT_FAILURE;
        } else {
            if (type == None)
                type = old_type;
            if (format == 0)
                format = old_format;
        }

        XFree(data.c);
    }

    if (type == None) {
        fprintf(stderr, "property %s doesn't exist, you need to specify "
                "its type and format\n", name);
        return EXIT_FAILURE;
    }

    data.c = calloc(nelements, sizeof(int32_t));

    for (i = 0; i < nelements; i++)
    {
        if (type == XA_INTEGER) {
            switch (format)
            {
                case 8:
                    data.c[i] = atoi(argv[2 + i]);
                    break;
                case 16:
                    data.s[i] = atoi(argv[2 + i]);
                    break;
                case 32:
                    data.l[i] = atoi(argv[2 + i]);
                    break;
                default:
                    fprintf(stderr, "unexpected size for property %s", name);
                    return EXIT_FAILURE;
            }
        } else if (type == float_atom) {
            if (format != 32) {
                fprintf(stderr, "unexpected format %d for property %s\n",
                        format, name);
                return EXIT_FAILURE;
            }
            *(float *)(data.l + i) = strtod(argv[2 + i], &endptr);
            if (endptr == argv[2 + i]) {
                fprintf(stderr, "argument %s could not be parsed\n", argv[2 + i]);
                return EXIT_FAILURE;
            }
        } else if (type == XA_ATOM) {
            if (format != 32) {
                fprintf(stderr, "unexpected format %d for property %s\n",
                        format, name);
                return EXIT_FAILURE;
            }
            data.l[i] = parse_atom(dpy, argv[2 + i]);
        } else {
            fprintf(stderr, "unexpected type for property %s\n", name);
            return EXIT_FAILURE;
        }
    }

    XIChangeProperty(dpy, info->deviceid, prop, type, format, PropModeReplace,
                          data.c, nelements);
    free(data.c);
    return EXIT_SUCCESS;
}
#endif

int list_props(Display *display, int argc, char *argv[], char *name,
               char *desc)
{
#ifdef HAVE_XI2
    if (xinput_version(display) == XI_2_Major)
        return list_props_xi2(display, argc, argv, name, desc);
#endif
    return list_props_xi1(display, argc, argv, name, desc);

}

int delete_prop(Display *display, int argc, char *argv[], char *name,
                char *desc)
{
#ifdef HAVE_XI2
    if (xinput_version(display) == XI_2_Major)
        return delete_prop_xi2(display, argc, argv, name, desc);
#endif
    return delete_prop_xi1(display, argc, argv, name, desc);

}

static int
do_set_prop(Display *display, Atom type, int format, int argc, char *argv[], char *name, char *desc)
{
#ifdef HAVE_XI2
    if (xinput_version(display) == XI_2_Major)
        return do_set_prop_xi2(display, type, format, argc, argv, name, desc);
#endif
    return do_set_prop_xi1(display, type, format, argc, argv, name, desc);
}

int
set_atom_prop(Display *dpy, int argc, char** argv, char* n, char *desc)
{
    return do_set_prop(dpy, XA_ATOM, 32, argc, argv, n, desc);
}

int
set_int_prop(Display *dpy, int argc, char** argv, char* n, char *desc)
{
    int          i;
    int          format;

    if (argc < 3)
    {
        fprintf(stderr, "Usage: xinput %s %s\n", n, desc);
        return EXIT_FAILURE;
    }

    format    = atoi(argv[2]);
    if (format != 8 && format != 16 && format != 32)
    {
        fprintf(stderr, "Invalid format %d\n", format);
        return EXIT_FAILURE;
    }

    for (i = 3; i < argc; i++)
        argv[i - 1] = argv[i];

    return do_set_prop(dpy, XA_INTEGER, format, argc - 1, argv, n, desc);
}

int
set_float_prop(Display *dpy, int argc, char** argv, char* n, char *desc)
{
    Atom float_atom = XInternAtom(dpy, "FLOAT", False);

    if (sizeof(float) != 4)
    {
	fprintf(stderr, "sane FP required\n");
	return EXIT_FAILURE;
    }

    return do_set_prop(dpy, float_atom, 32, argc, argv, n, desc);
}

int set_prop(Display *display, int argc, char *argv[], char *name,
             char *desc)
{
    Atom type = None;
    int format = 0;
    int i = 0, j;

    while (i < argc) {
        char *option = strchr(argv[i], '=');
        /* skip non-option arguments */
        if (strncmp(argv[i], "--", 2) || !option) {
            i++;
            continue;
        }

        if (!strncmp(argv[i], "--type=", strlen("--type="))) {
            if (!strcmp(option + 1, "int")) {
                type = XA_INTEGER;
            } else if (!strcmp(option + 1, "float")) {
                type = XInternAtom(display, "FLOAT", False);
                format = 32;
            } else if (!strcmp(option + 1, "atom")) {
                type = XA_ATOM;
                format = 32;
            } else {
                fprintf(stderr, "unknown property type %s\n", option + 1);
                return EXIT_FAILURE;
            }
        } else if (!strncmp(argv[i], "--format=", strlen("--format="))) {
            format = atoi(option + 1);
            if (format != 8 && format != 16 && format != 32) {
                fprintf(stderr, "invalid property format %s\n", option + 1);
                return EXIT_FAILURE;
            }
        } else {
            fprintf(stderr, "invalid option %s\n", argv[i]);
            return EXIT_FAILURE;
        }

        for (j = i; j + 1 < argc; j++)
            argv[j] = argv[j + 1];
        argc--;
    }

    return do_set_prop(display, type, format, argc, argv, name, desc);
}
