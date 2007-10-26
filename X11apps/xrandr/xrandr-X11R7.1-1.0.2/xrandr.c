/* $XdotOrg: app/xrandr/xrandr.c,v 1.6 2006/04/25 22:54:01 alanc Exp $
 * $XFree86: xc/programs/xrandr/xrandr.c,v 1.11 2002/10/14 18:01:43 keithp Exp $
 *
 * Copyright © 2001 Keith Packard, member of The XFree86 Project, Inc.
 * Copyright © 2002 Hewlett Packard Company, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard or HP not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard and HP makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD and HP DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Blame Jim Gettys for any bugs; he wrote most of the client side code,
 * and part of the server code for randr.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>	/* we share subpixel information */
#include <string.h>
#include <stdlib.h>

static char *program_name;

static char *direction[5] = {
  "normal", 
  "left", 
  "inverted", 
  "right",
  "\n"};

/* subpixel order */
static char *order[6] = {
  "unknown",
  "horizontal rgb",
  "horizontal bgr",
  "vertical rgb",
  "vertical bgr",
  "no subpixels"};


static void
usage(void)
{
  fprintf(stderr, "usage: %s [options]\n", program_name);
  fprintf(stderr, "  where options are:\n");
  fprintf(stderr, "  -display <display> or -d <display>\n");
  fprintf(stderr, "  -help\n");
  fprintf(stderr, "  -o <normal,inverted,left,right,0,1,2,3>\n");
  fprintf(stderr, "            or --orientation <normal,inverted,left,right,0,1,2,3>\n");
  fprintf(stderr, "  -q        or --query\n");
  fprintf(stderr, "  -s <size>/<width>x<height> or --size <size>/<width>x<height>\n");
  fprintf(stderr, "  -r <rate> or --rate <rate>\n");
  fprintf(stderr, "  -v        or --version\n");
  fprintf(stderr, "  -x        (reflect in x)\n");
  fprintf(stderr, "  -y        (reflect in y)\n");
  fprintf(stderr, "  --screen <screen>\n");
  fprintf(stderr, "  --verbose\n");
  
  exit(1);
  /*NOTREACHED*/
}

int
main (int argc, char **argv)
{
  Display       *dpy;
  XRRScreenSize *sizes;
  XRRScreenConfiguration *sc;
  int		nsize;
  int		nrate;
  short		*rates;
  Window	root;
  Status	status = RRSetConfigFailed;
  int		rot = -1;
  int		verbose = 0, query = 0;
  Rotation	rotation, current_rotation, rotations;
  XEvent	event;
  XRRScreenChangeNotifyEvent *sce;    
  char          *display_name = NULL;
  int 		i, j;
  SizeID	current_size;
  short		current_rate;
  int		rate = -1;
  int		size = -1;
  int		dirind = 0;
  int		setit = 0;
  int		screen = -1;
  int		version = 0;
  int		event_base, error_base;
  int		reflection = 0;
  int		width = 0, height = 0;
  int		have_pixel_size = 0;
  int		ret = 0;

  program_name = argv[0];
  if (argc == 1) query = 1;
  for (i = 1; i < argc; i++) {
    if (!strcmp ("-display", argv[i]) || !strcmp ("-d", argv[i])) {
      if (++i>=argc) usage ();
      display_name = argv[i];
      continue;
    }
    if (!strcmp("-help", argv[i])) {
      usage();
      continue;
    }
    if (!strcmp ("--verbose", argv[i])) {
      verbose = 1;
      continue;
    }

    if (!strcmp ("-s", argv[i]) || !strcmp ("--size", argv[i])) {
      if (++i>=argc) usage ();
      if (sscanf (argv[i], "%dx%d", &width, &height) == 2)
	have_pixel_size = 1;
      else {
	size = atoi (argv[i]);
	if (size < 0) usage();
      }
      setit = 1;
      continue;
    }

    if (!strcmp ("-r", argv[i]) || !strcmp ("--rate", argv[i])) {
      if (++i>=argc) usage ();
      rate = atoi (argv[i]);
      if (rate < 0) usage();
      setit = 1;
      continue;
    }

    if (!strcmp ("-v", argv[i]) || !strcmp ("--version", argv[i])) {
      version = 1;
      continue;
    }

    if (!strcmp ("-x", argv[i])) {
      reflection |= RR_Reflect_X;
      setit = 1;
      continue;
    }
    if (!strcmp ("-y", argv[i])) {
      reflection |= RR_Reflect_Y;
      setit = 1;
      continue;
    }
    if (!strcmp ("--screen", argv[i])) {
      if (++i>=argc) usage ();
      screen = atoi (argv[i]);
      if (screen < 0) usage();
      continue;
    }
    if (!strcmp ("-q", argv[i]) || !strcmp ("--query", argv[i])) {
      query = 1;
      continue;
    }
    if (!strcmp ("-o", argv[i]) || !strcmp ("--orientation", argv[i])) {
      char *endptr;
      if (++i>=argc) usage ();
      dirind = strtol(argv[i], &endptr, 0);
      if (*endptr != '\0') {
	for (dirind = 0; dirind < 4; dirind++) {
	  if (strcmp (direction[dirind], argv[i]) == 0) break;
	}
	if ((dirind < 0) || (dirind > 3))  usage();
      }
      rot = dirind;
      setit = 1;
      continue;
    }
    usage();
  }
  if (verbose) query = 1;

  dpy = XOpenDisplay (display_name);

  if (dpy == NULL) {
      fprintf (stderr, "Can't open display %s\n", XDisplayName(display_name));
      exit (1);
  }
  if (screen < 0)
    screen = DefaultScreen (dpy);
  if (screen >= ScreenCount (dpy)) {
    fprintf (stderr, "Invalid screen number %d (display has %d)\n",
	     screen, ScreenCount (dpy));
    exit (1);
  }

  root = RootWindow (dpy, screen);

  sc = XRRGetScreenInfo (dpy, root);

  if (sc == NULL) 
      exit (1);
  
  current_size = XRRConfigCurrentConfiguration (sc, &current_rotation);

  sizes = XRRConfigSizes(sc, &nsize);

  if (have_pixel_size) {
    for (size = 0; size < nsize; size++)
    {
      if (sizes[size].width == width && sizes[size].height == height)
	break;
    }
    if (size >= nsize) {
      fprintf (stderr,
	       "Size %dx%d not found in available modes\n", width, height);
      exit (1);
    }
  }
  else if (size < 0)
    size = current_size;

  if (rot < 0)
  {
    for (rot = 0; rot < 4; rot++)
	if (1 << rot == (current_rotation & 0xf))
	    break;
  }

  current_rate = XRRConfigCurrentRate (sc);

  if (rate < 0)
  {
    if (size == current_size)
	rate = current_rate;
    else
	rate = 0;
  }
  else
  {
    rates = XRRConfigRates (sc, size, &nrate);
    for (i = 0; i < nrate; i++)
      if (rate == rates[i])
	break;
    if (i == nrate) {
      fprintf (stderr, "Rate %d not available for this size\n", rate);
      exit (1);
    }
  }

  if (version) {
    int major_version, minor_version;
    XRRQueryVersion (dpy, &major_version, &minor_version);
    printf("Server reports RandR version %d.%d\n", 
	   major_version, minor_version);
  }

  if (query) {
    printf(" SZ:    Pixels          Physical       Refresh\n");
    for (i = 0; i < nsize; i++) {
      printf ("%c%-2d %5d x %-5d  (%4dmm x%4dmm )",
	   i == current_size ? '*' : ' ',
	   i, sizes[i].width, sizes[i].height,
	   sizes[i].mwidth, sizes[i].mheight);
      rates = XRRConfigRates (sc, i, &nrate);
      if (nrate) printf ("  ");
      for (j = 0; j < nrate; j++)
	printf ("%c%-4d",
		i == current_size && rates[j] == current_rate ? '*' : ' ',
		rates[j]);
      printf ("\n");
    }
  }

  rotations = XRRConfigRotations(sc, &current_rotation);

  rotation = 1 << rot ;
  if (query) {
    for (i = 0; i < 4; i ++) {
      if ((current_rotation >> i) & 1) 
	printf("Current rotation - %s\n", direction[i]);
    }

    printf("Current reflection - ");
    if (current_rotation & (RR_Reflect_X|RR_Reflect_Y))
    {
	if (current_rotation & RR_Reflect_X) printf ("X Axis ");
	if (current_rotation & RR_Reflect_Y) printf ("Y Axis");
    }
    else
	printf ("none");
    printf ("\n");
    

    printf ("Rotations possible - ");
    for (i = 0; i < 4; i ++) {
      if ((rotations >> i) & 1)  printf("%s ", direction[i]);
    }
    printf ("\n");

    printf ("Reflections possible - ");
    if (rotations & (RR_Reflect_X|RR_Reflect_Y))
    {
        if (rotations & RR_Reflect_X) printf ("X Axis ");
	if (rotations & RR_Reflect_Y) printf ("Y Axis");
    }
    else
	printf ("none");
    printf ("\n");
  }

  if (verbose) { 
    printf("Setting size to %d, rotation to %s\n",  size, direction[rot]);

    printf ("Setting reflection on ");
    if (reflection)
    {
	if (reflection & RR_Reflect_X) printf ("X Axis ");
	if (reflection & RR_Reflect_Y) printf ("Y Axis");
    }
    else
	printf ("neither axis");
    printf ("\n");

    if (reflection & RR_Reflect_X) printf("Setting reflection on X axis\n");

    if (reflection & RR_Reflect_Y) printf("Setting reflection on Y axis\n");
  }

  /* we should test configureNotify on the root window */
  XSelectInput (dpy, root, StructureNotifyMask);

  if (setit) XRRSelectInput (dpy, root,
			RRScreenChangeNotifyMask);
  if (setit) status = XRRSetScreenConfigAndRate (dpy, sc,
						 DefaultRootWindow (dpy), 
	       (SizeID) size, (Rotation) (rotation | reflection), rate, CurrentTime);
  
  XRRQueryExtension(dpy, &event_base, &error_base);

  if (setit && status == RRSetConfigFailed) {
     printf ("Failed to change the screen configuration!\n");
     ret = 1;
  }

  if (verbose && setit) {
    if (status == RRSetConfigSuccess)
      {
	while (1) {
	int spo;
	XNextEvent(dpy, (XEvent *) &event);
	
	printf ("Event received, type = %d\n", event.type);
	/* update Xlib's knowledge of the event */
	XRRUpdateConfiguration (&event);
	if (event.type == ConfigureNotify)
	  printf("Received ConfigureNotify Event!\n");

	switch (event.type - event_base) {
	case RRScreenChangeNotify:
	  sce = (XRRScreenChangeNotifyEvent *) &event;

	  printf("Got a screen change notify event!\n");
	  printf(" window = %d\n root = %d\n size_index = %d\n rotation %d\n", 
	       (int) sce->window, (int) sce->root, 
	       sce->size_index,  sce->rotation);
	  printf(" timestamp = %ld, config_timestamp = %ld\n",
	       sce->timestamp, sce->config_timestamp);
	  printf(" Rotation = %x\n", sce->rotation);
	  printf(" %d X %d pixels, %d X %d mm\n",
		 sce->width, sce->height, sce->mwidth, sce->mheight);
	  printf("Display width   %d, height   %d\n",
		 DisplayWidth(dpy, screen), DisplayHeight(dpy, screen));
	  printf("Display widthmm %d, heightmm %d\n", 
		 DisplayWidthMM(dpy, screen), DisplayHeightMM(dpy, screen));
	  spo = sce->subpixel_order;
	  if ((spo < 0) || (spo > 5))
	    printf ("Unknown subpixel order, value = %d\n", spo);
	  else printf ("new Subpixel rendering model is %s\n", order[spo]);
	  break;
	default:
	  if (event.type != ConfigureNotify) 
	    printf("unknown event received, type = %d!\n", event.type);
	}
	}
      }
  }
  XRRFreeScreenConfigInfo(sc);
  return(ret);
}
