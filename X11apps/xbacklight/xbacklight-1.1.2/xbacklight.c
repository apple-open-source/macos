/*
 * Copyright © 2007 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <ctype.h>
#include <string.h>
#include <unistd.h>

typedef enum { Get, Set, Inc, Dec } op_t;

static char *program_name;

static Atom backlight, backlight_new, backlight_legacy;

static void
usage (void)
{
    fprintf(stderr, "usage: %s [options]\n", program_name);
    fprintf(stderr, "  where options are:\n");
    fprintf(stderr, "  -display <display> or -d <display>\n");
    fprintf(stderr, "  -help\n");
    fprintf(stderr, "  -set <percentage> or = <percentage>\n");
    fprintf(stderr, "  -inc <percentage> or + <percentage>\n");
    fprintf(stderr, "  -dec <percentage> or - <percentage>\n");
    fprintf(stderr, "  -get\n");
    fprintf(stderr, "  -time <fade time in milliseconds>\n");
    fprintf(stderr, "  -steps <number of steps in fade>\n");
    /*NOTREACHED*/
    exit (1);
}

static long
backlight_get (Display *dpy, RROutput output)
{
    unsigned long   nitems;
    unsigned long   bytes_after;
    unsigned char   *prop;
    Atom	    actual_type;
    int		    actual_format;
    long	    value;
    
    backlight = backlight_new;
    if (!backlight ||
	XRRGetOutputProperty (dpy, output, backlight,
			      0, 4, False, False, None,
			      &actual_type, &actual_format,
			      &nitems, &bytes_after, &prop) != Success) {
	backlight = backlight_legacy;
	if (!backlight ||
	    XRRGetOutputProperty (dpy, output, backlight,
				  0, 4, False, False, None,
				  &actual_type, &actual_format,
				  &nitems, &bytes_after, &prop) != Success)
	    return -1;
    }

    if (actual_type != XA_INTEGER || nitems != 1 || actual_format != 32)
	value = -1;
    else
	value = *((long *) prop);
    XFree (prop);
    return value;
}

static void
backlight_set (Display *dpy, RROutput output, long value)
{
    XRRChangeOutputProperty (dpy, output, backlight, XA_INTEGER, 32,
			     PropModeReplace, (unsigned char *) &value, 1);
}

int
main (int argc, char **argv)
{
    char    *dpy_name = NULL;
    Display *dpy;
    int	    screen;
    int	    major, minor;
    op_t    op = Get;
    int	    value = 0;
    int	    i;
    int	    total_time = 200;	/* ms */
    int	    steps = 20;

    program_name = argv[0];

    for (i = 1; i < argc; i++)
    {
	if (!strcmp (argv[i], "-display") || !strcmp ("-d", argv[i]))
	{
	    if (++i >= argc) usage();
	    dpy_name = argv[i];
	    continue;
	}
	if (!strcmp (argv[i], "-set") || !strcmp (argv[i], "="))
	{
	    if (++i >= argc) usage();
	    op = Set;
	    value = atoi (argv[i]);
	    continue;
	}
	if (argv[i][0] == '=' && isdigit (argv[i][1]))
	{
	    op = Set;
	    value = atoi (argv[i] + 1);
	    continue;
	}
	if (!strcmp (argv[i], "-inc") || !strcmp (argv[i], "+"))
	{
	    if (++i >= argc) usage();
	    op = Inc;
	    value = atoi (argv[i]);
	    continue;
	}
	if (argv[i][0] == '+' && isdigit (argv[i][1]))
	{
	    op = Inc;
	    value = atoi (argv[i] + 1);
	    continue;
	}
	if (!strcmp (argv[i], "-dec") || !strcmp (argv[i], "-"))
	{
	    if (++i >= argc) usage();
	    op = Dec;
	    value = atoi (argv[i]);
	    continue;
	}
	if (argv[i][0] == '-' && isdigit (argv[i][1]))
	{
	    op = Dec;
	    value = atoi (argv[i] + 1);
	    continue;
	}
	if (!strcmp (argv[i], "-get") || !strcmp (argv[i], "-g"))
	{
	    op = Get;
	    continue;
	}
	if (!strcmp (argv[i], "-time"))
	{
	    if (++i >= argc) usage();
	    total_time = atoi (argv[i]);
	    continue;
	}
	if (!strcmp (argv[i], "-steps"))
	{
	    if (++i >= argc) usage();
	    steps = atoi (argv[i]);
	    continue;
	}
	if (!strcmp (argv[i], "-help") || !strcmp (argv[i], "-?"))
	{
	    usage ();
	}
	usage ();
    }
    dpy = XOpenDisplay (dpy_name);
    if (!dpy)
    {
	fprintf (stderr, "Cannot open display \"%s\"\n",
		 XDisplayName (dpy_name));
	exit (1);
    }
    if (!XRRQueryVersion (dpy, &major, &minor))
    {
	fprintf (stderr, "RandR extension missing\n");
	exit (1);
    }
    if (major < 1 || (major == 1 && minor < 2))
    {
	fprintf (stderr, "RandR version %d.%d too old\n", major, minor);
	exit (1);
    }

    backlight_new    = XInternAtom (dpy, "Backlight", True);
    backlight_legacy = XInternAtom (dpy, "BACKLIGHT", True);
    if (backlight_new == None && backlight_legacy == None)
    {
	fprintf (stderr, "No outputs have backlight property\n");
	exit (1);
    }
    for (screen = 0; screen < ScreenCount (dpy); screen++)
    {
	Window		    root = RootWindow (dpy, screen);
	XRRScreenResources  *resources = XRRGetScreenResources (dpy, root);
	int		    o;
	
	if (!resources) continue;
	for (o = 0; o < resources->noutput; o++)
	{
	    RROutput	output = resources->outputs[o];
	    XRRPropertyInfo *info;
	    double    	cur, new, step;
	    double	min, max;
	    double	set;

	    cur = backlight_get (dpy, output);
	    if (cur != -1)
	    {
		info = XRRQueryOutputProperty (dpy, output, backlight);
		if (info)
		{
		    if (info->range && info->num_values == 2)
		    {
			min = info->values[0];
			max = info->values[1];
			if (op == Get) {
			    printf ("%f\n", (cur - min) * 100 / (max - min));
			} else {
			    set = value * (max - min) / 100;
			    switch (op) {
			    case Set:
				new = min + set;
				break;
			    case Inc:
				new = cur + set;
				break;
			    case Dec:
				new = cur - set;
				break;
			    default:
				XSync (dpy, False);
				return 1;
			    }
			    if (new > max) new = max;
			    if (new < min) new = min;
			    step = (new - cur) / steps;
			    for (i = 0; i < steps && step != 0; i++)
			    {
				if (i == steps - 1)
				    cur = new;
				else
				    cur += step;
				backlight_set (dpy, output, (long) cur);
				XFlush (dpy);
				usleep (total_time * 1000 / steps);
			    }
			}
		    }
		    XFree (info);
		}
	    }
	}
							   
	XRRFreeScreenResources (resources);
    }
    XSync (dpy, False);

    return 0;
}
