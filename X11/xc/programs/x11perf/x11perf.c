/* $Xorg: x11perf.c,v 1.4 2000/08/17 19:54:10 cpqbld Exp $ */
/****************************************************************************
Copyright 1988, 1989 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

****************************************************************************/
/* $XFree86: xc/programs/x11perf/x11perf.c,v 3.8 2002/12/04 10:28:08 eich Exp $ */

#include <stdio.h>
#include <ctype.h>
#include <signal.h>

#ifndef VMS
#include <X11/Xatom.h>
#include <X11/Xos.h>
#else
#include <decw$include/Xatom.h>
#endif
#include "x11perf.h"
#include <X11/Xmu/SysUtil.h>

#include <time.h>
#define Time_t time_t
#include <stdlib.h>

/* Only for working on ``fake'' servers, for hardware that doesn't exist */
static Bool     drawToFakeServer = False;
static Pixmap   tileToQuery     = None;
static char *displayName;
int	abortTest;

typedef struct _RopNames { char	*name; int  rop; } RopNameRec, *RopNamePtr;

RopNameRec ropNames[] = {
	{ "clear",    	  GXclear },		/* 0 */
	{ "and",	  GXand },		/* src AND dst */
	{ "andReverse",	  GXandReverse }, 	/* src AND NOT dst */
	{ "copy",	  GXcopy },		/* src */
	{ "andInverted",  GXandInverted },	/* NOT src AND dst */
	{ "noop",	  GXnoop },		/* dst */
	{ "xor",	  GXxor },		/* src XOR dst */
	{ "or",		  GXor },		/* src OR dst */
	{ "nor",	  GXnor },		/* NOT src AND NOT dst */
	{ "equiv",	  GXequiv },		/* NOT src XOR dst */
	{ "invert",	  GXinvert },		/* NOT dst */
	{ "orReverse",	  GXorReverse },	/* src OR NOT dst */
	{ "copyInverted", GXcopyInverted },	/* NOT src */
	{ "orInverted",	  GXorInverted },	/* NOT src OR dst */
	{ "nand",	  GXnand },		/* NOT src OR NOT dst */
	{ "set",	  GXset }		/* 1 */
};

char *(visualClassNames)[] = {
    "StaticGray",
    "GrayScale",
    "StaticColor",
    "PseudoColor",
    "TrueColor",
    "DirectColor"
};

static Bool     labels		= False;
static int      repeat		= 5;
static int	seconds		= 5;

static Window   status;     /* Status window and GC */
static GC       tgc;
static int	HSx, HSy;

static double syncTime = 0.0;

static int saveargc;
static char **saveargv;

static int  numRops = 1;
static int  rops[16] = { GXcopy };
static int  numPlanemasks = 1;
static unsigned long planemasks[256] = { (unsigned long)~0 };

static char *foreground = NULL;
static char *background = NULL;
static char *ddbackground = NULL;
static int clips = 0;

static int numSubWindows = 7;
static unsigned long subWindows[] = {4, 16, 25, 50, 75, 100, 200, 0};

static int  fixedReps = 0;

static Bool *doit;

static XRectangle ws[] = {  /* Clip rectangles */
    {195, 195, 120, 120},
    { 45, 145, 120, 120},
    {345, 245, 120, 120},
    { 45, 275, 120, 120},
    {345, 115, 120, 120},
    {195, 325, 120, 120}

};
#define MAXCLIP     (sizeof(ws) / sizeof(ws[0]))
static Window clipWindows[MAXCLIP];
static Colormap cmap;
static int depth = -1;  /* -1 means use default depth */
static int vclass = -1; /* -1 means use CopyFromParent */

/* ScreenSaver state */
static XParmRec    xparms;
static int ssTimeout, ssInterval, ssPreferBlanking, ssAllowExposures;

/* Static functions */
static int GetWords(int argi, int argc, char **argv, char **wordsp, int *nump);
static int GetNumbers(int argi, int argc, char **argv, unsigned long *intsp, 
		      int *nump);
static int GetRops(int argi, int argc, char **argv, int *ropsp, int *nump);

/************************************************
*	    time related stuff			*
************************************************/

#ifdef VMS

typedef struct _vms_time {
    unsigned long low;
    unsigned long high;
}vms_time;

struct timeval {
    long tv_sec;        /* seconds since Jan. 1, 1970 */
    long tv_usec;  /* and microseconds */
};

struct timezone {
    int  tz_minuteswest;     /* of Greenwich */
    int  tz_dsttime;    /* type of dst correction to apply */
};


static int firsttime = True;
static vms_time basetime;

int gettimeofday(tp)
    struct timeval *tp;
{
    vms_time current_time, resultant;
    unsigned long mumble, foo;
    int status;

    if (firsttime) {
        sys$gettim(&basetime);
        firsttime = False;
    }
    sys$gettim(&current_time);
    resultant.high = current_time.high - basetime.high;
    resultant.low = current_time.low - basetime.low;
    if (current_time.low < basetime.low) {
        resultant.high -= 1;
    }
    status = lib$ediv( &(10000000), &resultant, &tp->tv_sec, &tp->tv_usec);
    tp->tv_usec /= 10;
    return 0;
}

#endif

static struct  timeval start;

static void 
PrintTime(void)
{
    Time_t t;

    t = time((Time_t *)NULL);
    printf("%s\n", ctime(&t));
}

static void 
InitTimes(void)
{
    X_GETTIMEOFDAY(&start);
}

static double 
ElapsedTime(double correction)
{
    struct timeval stop;

    X_GETTIMEOFDAY(&stop);
    if (stop.tv_usec < start.tv_usec) {
        stop.tv_usec += 1000000;
	stop.tv_sec -= 1;
    }
    return  (double)(stop.tv_usec - start.tv_usec) +
            (1000000.0 * (double)(stop.tv_sec - start.tv_sec)) - correction;
}

static double 
RoundTo3Digits(double d)
{
    /* It's kind of silly to print out things like ``193658.4/sec'' so just
       junk all but 3 most significant digits. */

    double exponent, sign;

    exponent = 1.0;
    /* the code below won't work if d should happen to be non-positive. */
    if (d < 0.0) {
	d = -d;
	sign = -1.0;
    } else
	sign = 1.0;
    if (d >= 1000.0) {
	do {
	    exponent *= 10.0;
	} while (d/exponent >= 1000.0);
	d = (double)((int) (d/exponent + 0.5));
	d *= exponent;
    } else {
	if (d != 0.0) {
	    while (d*exponent < 100.0) {
	        exponent *= 10.0;
	    }
	}
	d = (double)((int) (d*exponent + 0.5));
	d /= exponent;
    }
    return d * sign;
}


static void 
ReportTimes(double usecs, int n, char *str, int average)
{
    double msecsperobj, objspersec;

    if(usecs != 0.0)
    {
        msecsperobj = usecs / (1000.0 * (double)n);
        objspersec = (double) n * 1000000.0 / usecs;

        /* Round obj/sec to 3 significant digits.  Leave msec untouched, to
	   allow averaging results from several repetitions. */
        objspersec =  RoundTo3Digits(objspersec);

        if (average) {
	    printf("%7d trep @ %8.4f msec (%8.1f/sec): %s\n", 
		    n, msecsperobj, objspersec, str);
	} else {
	    printf("%7d reps @ %8.4f msec (%8.1f/sec): %s\n", 
	        n, msecsperobj, objspersec, str);
	}
    } else {
	printf("%6d %sreps @ 0.0 msec (unmeasurably fast): %s\n",
	    n, average ? "t" : "", str);
    }

}



/************************************************
*		Generic X stuff			*
************************************************/

static char *program_name;
static void usage(void);

/*
 * Get_Display_Name (argc, argv) Look for -display, -d, or host:dpy (obselete)
 * If found, remove it from command line.  Don't go past a lone -.
 */
static char *
Get_Display_Name(int *pargc, /* MODIFIED */
		 char **argv) /* MODIFIED */
{
    int     argc = *pargc;
    char    **pargv = argv+1;
    char    *displayname = NULL;
    int     i;

    for (i = 1; i != argc; i++) {
	char *arg = argv[i];

	if (!strcmp (arg, "-display") || !strcmp (arg, "-d")) {
	    if (++i >= argc) usage ();

	    displayname = argv[i];
	    *pargc -= 2;
	    continue;
	}
	if (!strcmp(arg,"-")) {
	    while (i<argc)  *pargv++ = argv[i++];
	    break;
	}
	*pargv++ = arg;
    }

    *pargv = NULL;
    return (displayname);
}


/*
 * GetVersion (argc, argv) Look for -v1.2, -v1.3, or -v1.4.
 * If found remove it from command line.  Don't go past a lone -.
 */

static Version 
GetVersion(int *pargc, /* MODIFIED */
	   char **argv)  /* MODIFIED */
{
    int     argc = *pargc;
    char    **pargv = argv+1;
    Version version = VERSION1_5;
    int     i;
    Bool    found = False;

    for (i = 1; i != argc; i++) {
	char *arg = argv[i];

	if (!strcmp (arg, "-v1.2")) {
	    version = VERSION1_2;
	    *pargc -= 1;
	    if (found) {
		fprintf(stderr, "Warning: multiple version specifications\n");
	    }
	    found = True;
	    continue;
	}
	if (!strcmp (arg, "-v1.3")) {
	    version = VERSION1_3;
	    *pargc -= 1;
	    if (found) {
		fprintf(stderr, "Warning: multiple version specifications\n");
	    }
	    found = True;
	    continue;
	}
	if (!strcmp (arg, "-v1.4")) {
	    version = VERSION1_4;
	    *pargc -= 1;
	    if (found) {
		fprintf(stderr, "Warning: multiple version specifications\n");
	    }
	    found = True;
	    continue;
	}
	if (!strcmp(arg,"-")) {
	    while (i<argc)  *pargv++ = argv[i++];
	    break;
	}
	*pargv++ = arg;
    }

    *pargv = NULL;
    return (version);
}



/*
 * Open_Display: Routine to open a display with correct error handling.
 */
static Display *
Open_Display(char *display_name)
{
    Display *d;

    d = XOpenDisplay(display_name);
    if (d == NULL) {
	fprintf (stderr, "%s:  unable to open display '%s'\n",
		 program_name, XDisplayName (display_name));
	exit(1);
    }

    return(d);
}


#ifdef SIGNALRETURNSINT
static int
#else
static void
#endif
Cleanup(int sig)
{
    abortTest = sig;
}

void
AbortTest(void)
{
    fflush(stdout);
    
    XSetScreenSaver(xparms.d, ssTimeout, ssInterval, ssPreferBlanking,
	ssAllowExposures);
    XFlush(xparms.d);
    exit (abortTest);
}

/************************************************
*		Performance stuff		*
************************************************/


static void 
usage(void)
{
    char    **cpp;
    int     i = 0;
    static char *help_message[] = {
"where options include:",
"    -display <host:display>   the X server to contact",
"    -sync                     do the tests in synchronous mode",
"    -pack                     pack rectangles right next to each other",
"    -repeat <n>               do tests <n> times (default = 5)",
"    -time <s>                 do tests for <s> seconds each (default = 5)",
/*
"    -draw                     draw after each test -- pmax only",
*/
"    -all                      do all tests",
"    -range <test1>[,<test2>]  like all, but do <test1> to <test2>",
"    -labels                   generate test labels for use by fillblnk",
"    -fg                       the foreground color to use",
"    -bg                       the background color to use",
"    -clips <default>          default number of clip windows per test",
"    -ddbg                     the background color to use for DoubleDash",
"    -rop <rop0 rop1 ...>      use the given rops to draw (default = GXcopy)",
"    -pm <pm0 pm1 ...>         use the given planemasks to draw (default = ~0)",
"    -depth <depth>            use a visual with <depth> planes per pixel",
"    -vclass <class>           the visual class to use (default = root)",
"    -reps <n>                 fix the rep count (default = auto scale)",
"    -subs <s0 s1 ...>         a list of the number of sub-windows to use",
"    -v1.2                     perform only v1.2 tests using old semantics",
"    -v1.3                     perform only v1.3 tests using old semantics",
"    -su                       request save unders on windows",
"    -bs <backing_store_hint>  WhenMapped or Always (default = NotUseful)",
NULL};

    fflush(stdout);
    fprintf(stderr, "usage: %s [-options ...]\n", program_name);
    for (cpp = help_message; *cpp; cpp++) {
	fprintf(stderr, "%s\n", *cpp);
    }
    while (test[i].option != NULL) {
	if (test[i].versions & xparms.version ) {
	    fprintf(stderr, "    %-24s   %s\n",
		test[i].option,
		test[i].label14 ? test[i].label14 : test[i].label);
	}
        i++;
    }
    fprintf(stderr, "\n");
    
    /* Print out original command line as the above usage message is so long */
    for (i = 0; i != saveargc; i++) {
	fprintf(stderr, "%s ", saveargv[i]);
    }
    fprintf(stderr, "\n\n");
    exit (1);
}

void 
NullProc(XParms xp, Parms p)
{
}

int 
NullInitProc(XParms xp, Parms p, int reps)
{
    return reps;
}

static void 
HardwareSync(XParms xp)
{
    /*
     * Some graphics hardware allows the server to claim it is done,
     * while in reality the hardware is busily working away.  So fetch
     * a pixel from the drawable that was drawn to, which should be
     * enough to make the server wait for the graphics hardware.
     */
    XImage *image;

    image = XGetImage(xp->d, xp->p ? xp->p : xp->w, HSx, HSy, 
		      1, 1, ~0, ZPixmap);
    if (image) XDestroyImage(image);
}

static void 
DoHardwareSync(XParms xp, Parms p, int reps)    
{
    int i;
    
    for (i = 0; i != reps; i++) {
	HardwareSync(xp);
	CheckAbort ();
    }
}

static Test syncTest = {
    "syncTime", "Internal test for finding how long HardwareSync takes", NULL,
    NullInitProc, DoHardwareSync, NullProc, NullProc, 
    V1_2FEATURE, NONROP, 0,
    {1}
};


static Window 
CreatePerfWindow(XParms xp, int x, int y, int width, int height)
{
    XSetWindowAttributes xswa;
    Window w;
/*
    Screen *s;
    int su;

    s = DefaultScreenOfDisplay(xp->d);
    su = XDoesBackingStore(s);
    printf("Backing store of screen returns %d\n", su);
    su = XDoesSaveUnders(s);
    printf("Save unders of screen returns %d\n", su);
    su = XPlanesOfScreen(s);
    printf("Planes of screen returns %d\n", su);
*/
    xswa.background_pixel = xp->background;
    xswa.border_pixel = xp->foreground;
    xswa.colormap = cmap;
    xswa.override_redirect = True;
    xswa.backing_store = xp->backing_store;
    xswa.save_under = xp->save_under;
    w = XCreateWindow(xp->d, DefaultRootWindow(xp->d), x, y, width, height, 1,
        xp->vinfo.depth, CopyFromParent, xp->vinfo.visual,
	CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect 
	| CWBackingStore | CWSaveUnder, &xswa);
    XMapWindow (xp->d, w);
    return w;
}


static void 
CreateClipWindows(XParms xp, int clips)
{
    int j;
    XWindowAttributes    xwa;

    (void) XGetWindowAttributes(xp->d, xp->w, &xwa);
    if (clips > MAXCLIP) clips = MAXCLIP;
    for (j = 0; j != clips; j++) {
	clipWindows[j] = CreatePerfWindow(xp,
	    xwa.x + ws[j].x, xwa.y + ws[j].y, ws[j].width, ws[j].height);
    }
} /* CreateClipWindows */


static void 
DestroyClipWindows(XParms xp, int clips)
{
    int j;

    if (clips > MAXCLIP) clips = MAXCLIP;
    for (j = 0; j != clips; j++) {
	XDestroyWindow(xp->d, clipWindows[j]);
    }
} /* DestroyClipWindows */


static double 
DoTest(XParms xp, Test *test, int reps)
{
    double  time;
    unsigned int ret_width, ret_height;

    /* Tell screen-saver to restart counting again.  See comments below for the
       XSetScreenSaver call. */
    XForceScreenSaver(xp->d, ScreenSaverReset);
    HardwareSync (xp); 
    InitTimes ();
    (*test->proc) (xp, &test->parms, reps);
    HardwareSync(xp);

    time = ElapsedTime(syncTime);
    CheckAbort ();
    if (drawToFakeServer)
        XQueryBestSize(xp->d, TileShape, tileToQuery,
		       32, 32, &ret_width, &ret_height);
    (*test->passCleanup) (xp, &test->parms);
    return time;
}


static int 
CalibrateTest(XParms xp, Test *test, int seconds, double *usecperobj)
{
#define goal    2500000.0   /* Try to get up to 2.5 seconds		    */
#define enough  2000000.0   /* But settle for 2.0 seconds		    */
#define tick      10000.0   /* Assume clock not faster than .01 seconds     */

    double  usecs;
    int     reps, didreps;  /* Reps desired, reps performed		    */
    int     exponent;

    /* Attempt to get an idea how long each rep lasts by getting enough
       reps to last more tan enough.  Then scale that up to the number of
       seconds desired.

       If init call to test ever fails, return False and test will be skipped.
    */

    if (fixedReps != 0) {
	return fixedReps;
    }
    reps = 1;
    for (;;) {
	XDestroySubwindows(xp->d, xp->w);
	XClearWindow(xp->d, xp->w);
	didreps = (*test->init) (xp, &test->parms, reps);
	CheckAbort ();
	if (didreps == 0) {
	    return 0;
	}
	if ( test->clips < clips )
	  test->clips = clips ;
	/* Create clip windows if requested */
	CreateClipWindows(xp, test->clips);
	HardwareSync(xp);
	InitTimes();
	(*test->proc) (xp, &test->parms, reps);
	HardwareSync(xp);
	usecs = ElapsedTime(syncTime);
	(*test->passCleanup) (xp, &test->parms);
	(*test->cleanup) (xp, &test->parms);
	DestroyClipWindows(xp, test->clips);
	CheckAbort ();

	if (didreps != reps) {
	    /* The test can't do the number of reps as we asked for.  
	       Give up */
	    *usecperobj = 
		usecs / (double)(didreps * test->parms.objects);
	    return didreps;
	}
	/* Did we go long enough? */
	if (usecs >= enough) break;

	/* Don't let too short a clock make new reps wildly high */
	if (usecs <= tick)reps = reps*10;
	else{
	    /* Try to get up to goal seconds. */
	    reps = (int) (goal * (double)reps / usecs) + 1;
	}
    }

    *usecperobj = usecs / (double) (reps * test->parms.objects);
    reps = (int) ((double)seconds * 1000000.0 * (double)reps / usecs) + 1;

    /* Now round reps up to 1 digit accuracy, so we don't get stupid-looking
       numbers of repetitions. */
    reps--;
    exponent = 1;
    while (reps > 9) {
	reps /= 10;
	exponent *= 10;
    }
    reps = (reps + 1) * exponent;
    return reps;
} /* CalibrateTest */

static void 
CreatePerfGCs(XParms xp, int func, unsigned long pm)
{
    XGCValues gcvfg, gcvbg, gcvddbg,gcvddfg;
    unsigned long	fg, bg, ddbg;

    fg = xp->foreground;
    bg = xp->background;
    ddbg = xp->ddbackground;
    gcvfg.graphics_exposures = False;
    gcvbg.graphics_exposures = False;
    gcvddfg.graphics_exposures = False;
    gcvddbg.graphics_exposures = False;
    gcvfg.plane_mask = pm;
    gcvbg.plane_mask = pm;
    gcvddfg.plane_mask = pm;
    gcvddbg.plane_mask = pm;
    gcvfg.function = func;
    gcvbg.function = func;
    gcvddfg.function = func;
    gcvddbg.function = func;
    
    if (func == GXxor) {
	/* Make test look good visually if possible */
	gcvbg.foreground = gcvfg.foreground = bg ^ fg;
	gcvbg.background = gcvfg.background = bg;
	/* Double Dash GCs (This doesn't make a huge amount of sense) */
	gcvddbg.foreground = gcvddfg.foreground = bg ^ fg;
	gcvddbg.background = gcvddfg.foreground = bg ^ ddbg;
    } else {
	gcvfg.foreground = fg;
	gcvfg.background = bg;
	gcvbg.foreground = bg;
	gcvbg.background = fg;
	gcvddfg.foreground = fg;
	gcvddfg.background = ddbg;
	gcvddbg.foreground = ddbg;
	gcvddbg.background = fg;
    }
    xp->fggc = XCreateGC(xp->d, xp->w,
	GCForeground | GCBackground | GCGraphicsExposures
      | GCFunction | GCPlaneMask, &gcvfg);
    xp->bggc = XCreateGC(xp->d, xp->w, 
	GCForeground | GCBackground | GCGraphicsExposures
      | GCFunction | GCPlaneMask, &gcvbg);
    xp->ddfggc = XCreateGC(xp->d, xp->w,
	GCForeground | GCBackground | GCGraphicsExposures
      | GCFunction | GCPlaneMask, &gcvddfg);
    xp->ddbggc = XCreateGC(xp->d, xp->w, 
	GCForeground | GCBackground | GCGraphicsExposures
      | GCFunction | GCPlaneMask, &gcvddbg);
}


static void 
DestroyPerfGCs(XParms xp)
{
    XFreeGC(xp->d, xp->fggc);
    XFreeGC(xp->d, xp->bggc);
    XFreeGC(xp->d, xp->ddfggc);
    XFreeGC(xp->d, xp->ddbggc);
}

static unsigned long 
AllocateColor(Display *display, char *name, unsigned long pixel)
{
    XColor      color;

    if (name != NULL) {
	/* Try to parse color name */
	if (XParseColor(display, cmap, name, &color)) {
	    if (XAllocColor(display, cmap, &color)) {
		pixel = color.pixel;
	    } else {
		(void) fprintf(stderr,
		    "Can't allocate colormap entry for color %s\n", name);
	    }
	} else {
	    if(*name >= '0' && *name <= '9')
		pixel = atoi(name);
	    else
		(void) fprintf(stderr, "Can't parse color name %s\n", name);
	}
    }
    return pixel;
} /* AllocateColor */


static void 
DisplayStatus(Display *d, char *message, char *test, int try)
{
    char    s[500];

    XClearWindow(d, status);
    sprintf(s, "%d %s %s", try, message, test);
    /* We should really look at the height, descent of the font, etc. but
       who cares.  This works. */
    XDrawString(d, status, tgc, 10, 13, s, strlen(s));
}


static void 
ProcessTest(XParms xp, Test *test, int func, unsigned long pm, char *label)
{
    double  time, totalTime;
    int     reps;
    int     j;

    xp->planemask = pm;
    CreatePerfGCs(xp, func, pm);
    DisplayStatus(xp->d, "Calibrating", label, 0);
    reps = CalibrateTest(xp, test, seconds, &time);
    if (reps != 0) {
	srand(1);	/* allow reproducible results */
	XDestroySubwindows(xp->d, xp->w);
	XClearWindow(xp->d, xp->w);
	reps = (*test->init) (xp, &test->parms, reps);
	if (abortTest)
	    AbortTest ();
	/*
	 * if using fixedReps then will not have done CalibrateTest so must
	 * check result of init for 0 here
	 */
	if(reps == 0){
	    DestroyPerfGCs(xp);
	    return;
	}
	/* Create clip windows if requested */
	CreateClipWindows(xp, test->clips);

	totalTime = 0.0;
	for (j = 0; j != repeat; j++) {
	    DisplayStatus(xp->d, "Testing", label, j+1);
	    time = DoTest(xp, test, reps);
	    if (abortTest)
		AbortTest ();
	    totalTime += time;
	    ReportTimes (time, reps * test->parms.objects,
		    label, False);
	}
	if (repeat > 1) {
	    ReportTimes(totalTime,
		repeat * reps * test->parms.objects,
		label, True);
	}
	(*test->cleanup) (xp, &test->parms);
	DestroyClipWindows(xp, test->clips);
    } else {
	/* Test failed to initialize properly */
    }
    printf ("\n");
    fflush(stdout);
    DestroyPerfGCs(xp);
} /* ProcessTest */

#define Strstr strstr

#define LABELP(i) (test[i].label14 && (xparms.version >= VERSION1_4) \
		        ? test[i].label14 : test[i].label)

int
main(int argc, char *argv[])
{
    int		i, j, n, skip;
    int		numTests;       /* Even though the linker knows, we don't. */
    char	hostname[100];
    Bool	foundOne = False;
    Bool	synchronous = False;
    XGCValues	tgcv;
    int		screen;
    int		rop, pm;
    int		window_y, window_x;
    XVisualInfo *vinfolist, vinfotempl;
    unsigned long vmask;

    /* Save away argv, argc, for usage to print out */
    saveargc = argc;
    saveargv = (char **) malloc(argc * sizeof(char *));
    for (i = 0; i != argc; i++) {
	saveargv[i] = argv[i];
    }

    xparms.pack = False;
    xparms.save_under = False;
    xparms.backing_store = NotUseful;

    /* Count number of tests */
    ForEachTest(numTests);
    doit = (Bool *)calloc(numTests, sizeof(Bool));

    /* Parse arguments */
    program_name = argv[0];
    displayName = Get_Display_Name (&argc, argv);
    xparms.version = GetVersion(&argc, argv);
    for (i = 1; i != argc; i++) {
	if (strcmp (argv[i], "-all") == 0) {
	    ForEachTest (j)
		doit[j] = test[j].versions & xparms.version;
	    foundOne = True;
	} else if (strcmp (argv[i], "-labels") == 0) {
	    labels = True;
	} else if (strcmp(argv[i], "-range") == 0) {
	    char *cp1;
	    char *cp2;
	    
	    if (argc <= ++i)
		usage();
	    cp1 = argv[i];
	    if (*cp1 == '-')
		cp1++;
	    for (cp2 = cp1; *cp2 != '\0' && *cp2 != ','; cp2++) {};
	    if (*cp2 == ',') {
		*cp2++ = '\0';
		if (*cp2 == '-')
		    cp2++;
	    } else {
		cp2 = "-";
	    }
	    ForEachTest (j) {
		if (strcmp (cp1, (test[j].option) + 1) == 0 &&
		    (test[j].versions & xparms.version)) {
		    int k = j;
		    do {
			doit[k] = test[j].versions & xparms.version;
		    } while (!(strcmp(cp2, (test[k].option + 1)) == 0 &&
			       (test[k].versions & xparms.version)) &&
			     test[++k].option != NULL);
		    if (*cp2 != '-' && test[k].option == NULL)
			usage();
		    break;
		}
	    }
	    if (test[j].option == NULL)
		usage();
	    foundOne = True;
	} else if (strcmp (argv[i], "-sync") == 0) {
	    synchronous = True;
	} else if (strcmp(argv[i], "-pack") == 0) {
	    xparms.pack = True;
	} else if (strcmp (argv[i], "-draw") == 0) {
	    drawToFakeServer = True;
	} else if (strcmp (argv[i], "-repeat") == 0) {
	    i++;
	    if (argc <= i)
		usage ();
	    repeat = atoi (argv[i]);
	    if (repeat <= 0)
	       usage ();
	} else if (strcmp (argv[i], "-time") == 0) {
	    i++;
	    if (argc <= i)
		usage ();
	    seconds = atoi (argv[i]);
	    if (seconds <= 0)
	       usage ();
	} else if (strcmp(argv[i], "-fg") == 0) {
	    i++;
	    if (argc <= i)
		usage ();
	    foreground = argv[i];
        } else if (strcmp(argv[i], "-bg") == 0) {
	    i++;
	    if (argc <= i)
		usage ();
	    background = argv[i];
	    if(ddbackground == NULL)
		ddbackground = argv[i];
	} else if (strcmp(argv[i], "-clips") == 0 ) {
	    i++;
	    if (argc <= i)
		usage ();
	    clips = atoi( argv[i] );
	} else if (strcmp(argv[i], "-ddbg") == 0) {
	    if (argc <= i)
		usage ();
	    i++;
	    ddbackground = argv[i];
	} else if (strcmp(argv[i], "-rop") == 0) {
	    skip = GetRops (i+1, argc, argv, rops, &numRops);
	    i += skip;
	} else if (strcmp(argv[i], "-pm") == 0) {
	    skip = GetNumbers (i+1, argc, argv, planemasks, &numPlanemasks);
	    i += skip;
	} else if (strcmp(argv[i], "-xor") == 0) {
	    numRops = 1;
	    rops[0] = GXxor;
	} else if (strcmp (argv[i], "-both") == 0) {
	    numRops = 2;
	    rops[0] = GXcopy;
	    rops[1] = GXxor;
	} else if (strcmp(argv[i], "-reps") == 0) {
	    i++;
	    if (argc <= i)
		usage ();
	    fixedReps = atoi (argv[i]);
	    if (fixedReps <= 0)
		usage ();
        } else if (strcmp(argv[i], "-depth") == 0) {
	    i++;
	    if (argc <= i)
                usage ();
            depth = atoi(argv[i]);
            if (depth <= 0)
		usage ();
        } else if (strcmp(argv[i], "-vclass") == 0) {
	    i++;
	    if (argc <= i)
                usage ();
	    for (j = StaticGray; j <= DirectColor; j++) {
		if (strcmp(argv[i], visualClassNames[j]) == 0) {
		    vclass = j;
		    break;
		}
	    }
            if (vclass < 0)
		usage ();
	} else if (strcmp(argv[i], "-subs") == 0) {
	    skip = GetNumbers (i+1, argc, argv, subWindows, &numSubWindows);
	    i += skip;
	} else if (strcmp(argv[i], "-v1.2") == 0) {
	    xparms.version = VERSION1_2;
	} else if (strcmp(argv[i], "-v1.3") == 0) {
	    xparms.version = VERSION1_3;
	} else if (strcmp(argv[i], "-su") == 0) {
	    xparms.save_under = True;
	} else if (strcmp(argv[i], "-bs") == 0) {
	    i++;
	    if (argc <= i)
		usage ();
	    if (strcmp(argv[i], "WhenMapped") == 0) {
	      xparms.backing_store = WhenMapped;
	    } else if (strcmp(argv[i], "Always") == 0) {
	      xparms.backing_store = Always;
	    } else usage ();
	} else {
	    int len,found;
	    ForEachTest (j) {
		if (strcmp (argv[i], test[j].option) == 0 &&
		    (test[j].versions & xparms.version)) {
		    doit[j] = True;
		    goto LegalOption;
		}
	    }
	    found = False;
	    len = strlen(argv[i]);
	    if(len>=3)
	    ForEachTest (j) {
		if (Strstr (test[j].option, argv[i]+1) != 0) {
		    fprintf(stderr,"	-> %s	%s\n", test[j].option, LABELP(j));
		    doit[j] = found = True;
		}
	    }
	    if(!found)
	    ForEachTest (j) {
		if (Strstr (LABELP(j), argv[i]+1) != 0) {
		    fprintf(stderr,"	-> %s	%s\n", test[j].option, LABELP(j));
		    doit[j] = found = True;
		}
	    }
	    if(!found)
		usage ();
	LegalOption: 
		foundOne = True;
	}
    }

    if (labels) {
	/* Just print out list of tests for use with .sh programs that
	   assemble data from different x11perf runs into a nice format */
	ForEachTest (i) {
	    int child;
	    if (doit[i]) {
		switch (test[i].testType) {
		    case NONROP:
			printf ("%s\n", LABELP(i));
			break;
    
		    case ROP:
			/* Run it through all specified rops and planemasks */
			for (rop = 0; rop < numRops; rop++) {
			    for (pm = 0; pm < numPlanemasks; pm++) {
				if (planemasks[pm] == ~0) {
				    if (rops[rop] == GXcopy) {
					printf ("%s\n", LABELP(i));
				    } else {
					printf ("(%s) %s\n",
					    ropNames[rops[rop]].name,
					    LABELP(i));
				    }
				} else {
				    printf ("(%s 0x%lx) %s\n",
					    ropNames[rops[rop]].name,
					    planemasks[pm],
					    LABELP(i));
				}
			    } /* for pm */
			} /* for rop */
			break;
		    
		    case PLANEMASK:
			/* Run it through all specified planemasks */
			for (pm = 0; pm < numPlanemasks; pm++) {
			    if (planemasks[pm] == ~0) {
				printf ("%s\n", LABELP(i));
			    } else {
				printf ("(0x%lx) %s\n",
					planemasks[pm],
					LABELP(i));
			    }
			} /* for pm */
			break;
		    
		    case WINDOW:
			for (child = 0; child != numSubWindows; child++) {
			    printf ("%s (%ld kids)\n",
				LABELP(i), subWindows[child]);
			}
			break;
		} /* switch */
	    }
	}
	exit(0);
    }

    if (!foundOne)
	usage ();
    xparms.d = Open_Display (displayName);
    screen = DefaultScreen(xparms.d);

    /* get visual info of default visual */
    vmask = VisualIDMask | VisualScreenMask;
    vinfotempl.visualid = XVisualIDFromVisual(XDefaultVisual(xparms.d, screen));
    vinfotempl.screen = screen;
    vinfolist = XGetVisualInfo(xparms.d, vmask, &vinfotempl, &n);
    if (!vinfolist || n != 1) {
	fprintf (stderr, "%s: can't get visual info of default visual\n",
	    program_name);
	exit(1);
    }

    if (depth == -1 && vclass == -1) {
	/* use the default visual and colormap */
	xparms.vinfo = *vinfolist;
	cmap = XDefaultColormap(xparms.d, screen);
    } else {
	/* find the specified visual */
	int errorDepth = vinfolist[0].depth;
	int errorClass = vinfolist[0].class;

	vmask = VisualScreenMask;
	vinfotempl.screen = screen;
	if (depth >= 0) {
	    vinfotempl.depth = depth;
	    vmask |= VisualDepthMask;
	    errorDepth = depth;
	}
	if (vclass >= 0) {
	    vinfotempl.class = vclass;
	    vmask |= VisualClassMask;
	    errorClass = vclass;
	}
	vinfolist = XGetVisualInfo(xparms.d, vmask, &vinfotempl, &n);
	if (!vinfolist) {
	    fprintf (stderr,
		"%s: can't find a visual of depth %d and class %s\n",
		program_name, errorDepth, visualClassNames[errorClass]);
	    exit(1);
	}
	xparms.vinfo = *vinfolist;  /* use the first one in list */
	if (xparms.vinfo.visualid ==
	    XVisualIDFromVisual(XDefaultVisual(xparms.d, screen))) {
	    /* matched visual is same as default visual */
	    cmap = XDefaultColormap(xparms.d, screen);
	} else {
	    cmap = XCreateColormap(xparms.d, DefaultRootWindow(xparms.d),
		xparms.vinfo.visual, AllocNone);
	    /* since this is not default cmap, must force color allocation */
	    if (!foreground) foreground = "Black";
	    if (!background) background = "White";
	    XInstallColormap(xparms.d, cmap);
	}
    }
    xparms.cmap = cmap;
    
    printf("x11perf - X11 performance program, version %s\n",
	   xparms.version & VERSION1_5 ? "1.5" :
	   xparms.version & VERSION1_4 ? "1.4" :
	   xparms.version & VERSION1_3 ? "1.3" :
	   "1.2"
	   );
    XmuGetHostname(hostname, 100);
    printf ("%s server version %d on %s\nfrom %s\n",
	    ServerVendor (xparms.d), VendorRelease (xparms.d),
	    DisplayString (xparms.d), hostname);
    PrintTime ();

    /* Force screen out of screen-saver mode, grab current data, and set
       time to blank to 8 hours.  We should just be able to turn the screen-
       saver off, but this causes problems on some servers.  We also reset
       the screen-saver timer each test, as 8 hours is about the maximum time
       we can use, and that isn't long enough for some X terminals using a
       serial protocol to finish all the tests.  As long as the tests run to 
       completion, the old screen-saver values are restored. */
    XForceScreenSaver(xparms.d, ScreenSaverReset);
    XGetScreenSaver(xparms.d, &ssTimeout, &ssInterval, &ssPreferBlanking,
	&ssAllowExposures);
    (void) signal(SIGINT, Cleanup); /* ^C */
#ifdef SIGQUIT
    (void) signal(SIGQUIT, Cleanup);
#endif
    (void) signal(SIGTERM, Cleanup);
#ifdef SIGHUP
    (void) signal(SIGHUP, Cleanup);
#endif
    XSetScreenSaver(xparms.d, 8 * 3600, ssInterval, ssPreferBlanking, 
	ssAllowExposures);

    if (drawToFakeServer) {
        tileToQuery =
	    XCreatePixmap(xparms.d, DefaultRootWindow (xparms.d), 32, 32, 1);
    }


    xparms.foreground =
	AllocateColor(xparms.d, foreground, BlackPixel(xparms.d, screen));
    xparms.background =
	AllocateColor(xparms.d, background, WhitePixel(xparms.d, screen));
    xparms.ddbackground =
	AllocateColor(xparms.d, ddbackground, WhitePixel(xparms.d, screen));
    window_x = 2;
    if (DisplayWidth(xparms.d, screen) < WIDTH + window_x + 1)
	window_x = -1;
    window_y = 2;
    if (DisplayHeight(xparms.d, screen) < HEIGHT + window_y + 1)
	window_y = -1;
    xparms.w = CreatePerfWindow(&xparms, window_x, window_y, WIDTH, HEIGHT);
    HSx = WIDTH-1;
    if (window_x + 1 + WIDTH > DisplayWidth(xparms.d, screen))
	HSx = DisplayWidth(xparms.d, screen) - (1 + window_x + 1);
    HSy = HEIGHT-1;
    if (window_y + 1 + HEIGHT > DisplayHeight(xparms.d, screen))
	HSy = DisplayHeight(xparms.d, screen) - (1 + window_y + 1);
    status = CreatePerfWindow(&xparms, window_x, HEIGHT+5, WIDTH, 20);
    tgcv.foreground = 
	AllocateColor(xparms.d, "black", BlackPixel(xparms.d, screen));
    tgcv.background = 
	AllocateColor(xparms.d, "white", WhitePixel(xparms.d, screen));
    tgc = XCreateGC(xparms.d, status, GCForeground | GCBackground, &tgcv);
   
    xparms.p = (Pixmap)0;

    if (synchronous)
	XSynchronize (xparms.d, True);

    /* Get mouse pointer out of the way of the performance window.  On
       software cursor machines it will slow graphics performance.  On
       all current MIT-derived servers it will slow window 
       creation/configuration performance. */
    XWarpPointer(xparms.d, None, status, 0, 0, 0, 0, WIDTH+32, 20+32);

    /* Figure out how long to call HardwareSync, so we can adjust for that
       in our total elapsed time */
    (void) CalibrateTest(&xparms, &syncTest, 1, &syncTime);
    printf("Sync time adjustment is %6.4f msecs.\n\n", syncTime/1000);

    ForEachTest (i) {
	int child;
	char label[200];

	if (doit[i] && (test[i].versions & xparms.version)) {
	    switch (test[i].testType) {
	        case NONROP:
		    /* Simplest...just run it once */
		    strcpy (label, LABELP(i));
		    ProcessTest(&xparms, &test[i], GXcopy, ~0L, label);
		    break;

		case ROP:
		    /* Run it through all specified rops and planemasks */
		    for (rop = 0; rop < numRops; rop++) {
			for (pm = 0; pm < numPlanemasks; pm++) {
			    if (planemasks[pm] == ~0) {
				if (rops[rop] == GXcopy) {
				    sprintf (label, "%s", LABELP(i));
				} else {
				    sprintf (label, "(%s) %s",
					ropNames[rops[rop]].name,
					LABELP(i));
				}
			    } else {
				sprintf (label, "(%s 0x%lx) %s",
					ropNames[rops[rop]].name,
					planemasks[pm],
					LABELP(i));
			    }
			    ProcessTest(&xparms, &test[i], rops[rop],
				        planemasks[pm], label);
			} /* for pm */
		    } /* for rop */
		    break;
		
		case PLANEMASK:
		    /* Run it through all specified planemasks */
		    for (pm = 0; pm < numPlanemasks; pm++) {
			if (planemasks[pm] == ~0) {
			    sprintf (label, "%s", LABELP(i));
			} else {
			    sprintf (label, "(0x%lx) %s",
				    planemasks[pm],
				    LABELP(i));
			}
			ProcessTest(&xparms, &test[i], GXcopy,
				    planemasks[pm], label);
		    } /* for pm */
		    break;
		
		case WINDOW:
		    /* Loop through number of children array */
		    for (child = 0; child != numSubWindows; child++) {
			test[i].parms.objects = subWindows[child];
			sprintf(label, "%s (%d kids)",
			    LABELP(i), test[i].parms.objects);
			ProcessTest(&xparms, &test[i], GXcopy, ~0L, label);
		    }
		    break;
	    } /* switch */
	} /* if doit */
    } /* ForEachTest */

    XFreeGC(xparms.d, tgc);
    XDestroyWindow(xparms.d, xparms.w);
    XFree(vinfolist);
    if (drawToFakeServer)
      XFreePixmap(xparms.d, tileToQuery);
    /* Restore ScreenSaver to original state. */
    XSetScreenSaver(xparms.d, ssTimeout, ssInterval, ssPreferBlanking,
	ssAllowExposures);
    XCloseDisplay(xparms.d);
    free(saveargv);
    free(doit);
    exit(0);
}

static int
GetWords (int argi, int argc, char **argv, char **wordsp, int *nump)
{
    int	    count;

    if (argc <= argi)
	usage();
    count = 0;
    while (argv[argi] && *(argv[argi]) != '-') {
	*wordsp++ = argv[argi];
	++argi;
	count++;
    }
    *nump = count;
    return count;
}

static long
atox (char *s)
{
    long   v, c = 0;

    v = 0;
    while (*s) {
	if ('0' <= *s && *s <= '9')
	    c = *s - '0';
	else if ('a' <= *s && *s <= 'f')
	    c = *s - 'a' + 10;
	else if ('A' <= *s && *s <= 'F')
	    c = *s - 'A' + 10;
	v = v * 16 + c;
	s++;
    }
    return v;
}

static int 
GetNumbers (int argi, int argc, char **argv, unsigned long *intsp, int *nump)
{
    char    *words[256];
    int	    count;
    int	    i;
    int	    flip;

    count = GetWords (argi, argc, argv, words, nump);
    for (i = 0; i < count; i++) {
	flip = 0;
	if (!strncmp (words[i], "~", 1)) {
	    words[i]++;
	    flip = ~0;
	}
	if (!strncmp (words[i], "0x", 2))
	    intsp[i] = atox(words[i] + 2) ^ flip;
	else
	    intsp[i] = atoi (words[i]) ^ flip;
    }
    return count;
}

static int
GetRops (int argi, int argc, char **argv, int *ropsp, int *nump)
{
    char    *words[256];
    int	    count;
    int	    i;
    int	    rop;

    count = GetWords (argi, argc, argv, words, nump);
    for (i = 0; i < count; i++) {
	if (!strncmp (words[i], "GX", 2))
	    words[i] += 2;
	if (!strcmp (words[i], "all")) {
	    for (i = 0; i < 16; i++)
		ropsp[i] = ropNames[i].rop;
	    *nump = 16;
	    break;
	}
	for (rop = 0; rop < 16; rop++) {
	    if (!strcmp (words[i], ropNames[rop].name)) {
		ropsp[i] = ropNames[rop].rop;
		break;
	    }
	}
	if (rop == 16) {
	    usage ();
	    fprintf (stderr, "unknown rop name %s\n", words[i]);
	}
    }
    return count;
}
