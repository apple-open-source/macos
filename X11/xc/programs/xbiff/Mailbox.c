/* $XConsortium: Mailbox.c,v 1.64 94/04/17 20:43:26 rws Exp $ */
/*

Copyright (c) 1988  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
/* $XFree86: xc/programs/xbiff/Mailbox.c,v 1.5 2001/10/28 03:34:25 tsi Exp $ */

/*
 * Author:  Jim Fulton, MIT X Consortium
 *
 * I recommend that you use the new mailfull and mailempty bitmaps instead of
 * the ugly mailboxes:
 *
 *         XBiff*fullPixmap:  mailfull
 *         XBiff*emptyPixmap:  mailempty
 */

#include <X11/IntrinsicP.h>		/* for toolkit stuff */
#include <X11/StringDefs.h>		/* for useful atom names */
#include <X11/cursorfont.h>		/* for cursor constants */
#include <X11/Xosdefs.h>		/* for X_NOT_POSIX def */
#include <stdlib.h>
#ifdef WIN32
#include <X11/Xw32defs.h>
#else
#include <pwd.h>			/* for getting username */
#endif
#include <sys/stat.h>			/* for stat() ** needs types.h ***/
#include <stdio.h>			/* for printing error messages */
#include <unistd.h>

#ifndef X_NOT_POSIX
#ifdef _POSIX_SOURCE
# include <sys/wait.h>
#else
#define _POSIX_SOURCE
# include <sys/wait.h>
#undef _POSIX_SOURCE
#endif
# define waitCode(w)	WEXITSTATUS(w)
# define waitSig(w)	WIFSIGNALED(w)
typedef int		waitType;
# define INTWAITTYPE
#else /* ! X_NOT_POSIX */
#ifdef SYSV
# define waitCode(w)	(((w) >> 8) & 0x7f)
# define waitSig(w)	((w) & 0xff)
typedef int		waitType;
# define INTWAITTYPE
#else
#ifdef WIN32
#include <process.h>
# define INTWAITTYPE
typedef int		waitType;
# define waitCode(w)	(w)
# define waitSig(w)	(0)
#else
# include	<sys/wait.h>
# define waitCode(w)	((w).w_T.w_Retcode)
# define waitSig(w)	((w).w_T.w_Termsig)
typedef union wait	waitType;
#endif /* WIN32 else */
#endif /* SYSV else */
#endif /* ! X_NOT_POSIX else */

#include <X11/bitmaps/mailfull>		/* for flag up (mail present) bits */
#include <X11/bitmaps/mailempty>	/* for flag down (mail not here) */

#include <X11/Xaw/XawInit.h>
#include "MailboxP.h"		/* for implementation mailbox stuff */
#include <X11/Xmu/Drawing.h>
#include <X11/extensions/shape.h>

/*
 * The default user interface is to have the mailbox turn itself off whenever
 * the user presses a button in it.  Expert users might want to make this 
 * happen on EnterWindow.  It might be nice to provide support for some sort of
 * exit callback so that you can do things like press q to quit.
 */

static char defaultTranslations[] = 
  "<ButtonPress>:  unset()";

static void Check(), Set(), Unset();

static XtActionsRec actionsList[] = { 
    { "check",	Check },
    { "unset",	Unset },
    { "set",	Set },
};


/* Initialization of defaults */

#define offset(field) XtOffsetOf(MailboxRec, mailbox.field)
#define goffset(field) XtOffsetOf(WidgetRec, core.field)

static Dimension defDim = 48;
static Pixmap nopix = None;

static XtResource resources[] = {
    { XtNwidth, XtCWidth, XtRDimension, sizeof (Dimension), 
	goffset (width), XtRDimension, (XtPointer)&defDim },
    { XtNheight, XtCHeight, XtRDimension, sizeof (Dimension),
	goffset (height), XtRDimension, (XtPointer)&defDim },
    { XtNupdate, XtCInterval, XtRInt, sizeof (int),
	offset (update), XtRString, "30" },
    { XtNforeground, XtCForeground, XtRPixel, sizeof (Pixel),
	offset (foreground_pixel), XtRString, XtDefaultForeground },
    { XtNfile, XtCFile, XtRString, sizeof (String),
	offset (filename), XtRString, NULL },
    { XtNcheckCommand, XtCCheckCommand, XtRString, sizeof(char*),
	offset (check_command), XtRString, NULL},
    { XtNvolume, XtCVolume, XtRInt, sizeof(int),
	offset (volume), XtRString, "33"},
    { XtNonceOnly, XtCBoolean, XtRBoolean, sizeof(Boolean),
	offset (once_only), XtRImmediate, (XtPointer)False },
    { XtNfullPixmap, XtCPixmap, XtRBitmap, sizeof(Pixmap),
	offset (full.bitmap), XtRString, "flagup" },
    { XtNfullPixmapMask, XtCPixmapMask, XtRBitmap, sizeof(Pixmap),
	offset (full.mask), XtRBitmap, (XtPointer) &nopix },
    { XtNemptyPixmap, XtCPixmap, XtRBitmap, sizeof(Pixmap),
	offset (empty.bitmap), XtRString, "flagdown" },
    { XtNemptyPixmapMask, XtCPixmapMask, XtRBitmap, sizeof(Pixmap),
	offset (empty.mask), XtRBitmap, (XtPointer) &nopix },
    { XtNflip, XtCFlip, XtRBoolean, sizeof(Boolean),
	offset (flipit), XtRString, "true" },
    { XtNshapeWindow, XtCShapeWindow, XtRBoolean, sizeof(Boolean),
        offset (shapeit), XtRString, "false" },
};

#undef offset
#undef goffset

static void GetMailFile(), CloseDown();
static void check_mailbox(), redraw_mailbox(), beep();
static void Initialize(), Realize(), Destroy(), Redisplay();
static Boolean SetValues();

MailboxClassRec mailboxClassRec = {
    { /* core fields */
    /* superclass		*/	(WidgetClass) &simpleClassRec,
    /* class_name		*/	"Mailbox",
    /* widget_size		*/	sizeof(MailboxRec),
    /* class_initialize		*/	XawInitializeWidgetSet,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	Realize,
    /* actions			*/	actionsList,
    /* num_actions		*/	XtNumber(actionsList),
    /* resources		*/	resources,
    /* resource_count		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	Destroy,
    /* resize			*/	NULL,
    /* expose			*/	Redisplay,
    /* set_values		*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	defaultTranslations,
    /* query_geometry		*/	XtInheritQueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
    },
    { /* simple fields */
    /* change_sensitive         */	XtInheritChangeSensitive
    },
    { /* mailbox fields */
    /* ignore                   */	0
    }
};

WidgetClass mailboxWidgetClass = (WidgetClass) &mailboxClassRec;


/*
 * widget initialization
 */

static GC get_mailbox_gc (w)
    MailboxWidget w;
{
    XtGCMask valuemask;
    XGCValues xgcv;

    valuemask = GCForeground | GCBackground | GCFunction | GCGraphicsExposures;
    xgcv.foreground = w->mailbox.foreground_pixel;
    xgcv.background = w->core.background_pixel;
    xgcv.function = GXcopy;
    xgcv.graphics_exposures = False;	/* this is Bool, not Boolean */
    return (XtGetGC ((Widget) w, valuemask, &xgcv));
}


/* ARGSUSED */
static void Initialize (request, new, args, num_args)
    Widget request, new;
    ArgList args;
    Cardinal *num_args;
{
    MailboxWidget w = (MailboxWidget) new;
    int shape_event_base, shape_error_base;

    if (w->core.width <= 0) w->core.width = 1;
    if (w->core.height <= 0) w->core.height = 1;

    if (w->mailbox.shapeit && !XShapeQueryExtension (XtDisplay (w),
						     &shape_event_base,
						     &shape_error_base))
      w->mailbox.shapeit = False;
    w->mailbox.shape_cache.mask = None;
    w->mailbox.gc = get_mailbox_gc (w);
    w->mailbox.interval_id = (XtIntervalId) 0;
    w->mailbox.full.pixmap = None;
    w->mailbox.empty.pixmap = None;
    w->mailbox.flag_up = FALSE;
    w->mailbox.last_size = 0;
    if (!w->mailbox.filename) GetMailFile (w);
    return;
}


/*
 * action procedures
 */

/*
 * pretend there is new mail; put widget in flagup state
 */

/* ARGSUSED */
static void Set (gw, event, params, nparams)
    Widget gw;
    XEvent *event;
    String *params;
    Cardinal *nparams;
{
    MailboxWidget w = (MailboxWidget) gw;

    w->mailbox.last_size = -1;

    check_mailbox (w, TRUE, FALSE);	/* redraw, no reset */

    return;
}


/*
 * ack the existing mail; put widget in flagdown state
 */

/* ARGSUSED */
static void Unset (gw, event, params, nparams)
    Widget gw;
    XEvent *event;
    String *params;
    Cardinal *nparams;
{
    MailboxWidget w = (MailboxWidget) gw;

    check_mailbox (w, TRUE, TRUE);	/* redraw, reset */

    return;
}


/*
 * look to see if there is new mail; if so, Set, else Unset
 */

/* ARGSUSED */
static void Check (gw, event, params, nparams)
    Widget gw;
    XEvent *event;
    String *params;
    Cardinal *nparams;
{
    MailboxWidget w = (MailboxWidget) gw;

    check_mailbox (w, TRUE, FALSE);	/* redraw, no reset */

    return;
}


/* ARGSUSED */
static void clock_tic (client_data, id)
    XtPointer client_data;
    XtIntervalId *id;
{
    MailboxWidget w = (MailboxWidget) client_data;

    check_mailbox (w, FALSE, FALSE);	/* no redraw, no reset */

    /*
     * and reset the timer
     */

    w->mailbox.interval_id =
	XtAppAddTimeOut (XtWidgetToApplicationContext((Widget) w),
			 w->mailbox.update * 1000, clock_tic, client_data);

    return;
}

static Pixmap make_pixmap (dpy, w, bitmap, depth, flip, widthp, heightp)
    Display *dpy;
    MailboxWidget w;
    Pixmap bitmap;
    Boolean flip;
    int depth;
    int *widthp, *heightp;
{
    Window root;
    int x, y;
    unsigned int width, height, bw, dep;
    unsigned long fore, back;

    if (!XGetGeometry (dpy, bitmap, &root, &x, &y, &width, &height, &bw, &dep))
      return None;

    *widthp = (int) width;
    *heightp = (int) height;
    if (flip) {
	fore = w->core.background_pixel;
	back = w->mailbox.foreground_pixel;
    } else {
	fore = w->mailbox.foreground_pixel;
	back = w->core.background_pixel;
    }
    return XmuCreatePixmapFromBitmap (dpy, w->core.window, bitmap, 
				      width, height, depth, fore, back);
}

static void Realize (gw, valuemaskp, attr)
    Widget gw;
    XtValueMask *valuemaskp;
    XSetWindowAttributes *attr;
{
    MailboxWidget w = (MailboxWidget) gw;
    register Display *dpy = XtDisplay (w);
    int depth = w->core.depth;

    *valuemaskp |= (CWBitGravity | CWCursor);
    attr->bit_gravity = ForgetGravity;
    attr->cursor = XCreateFontCursor (dpy, XC_top_left_arrow);

    (*mailboxWidgetClass->core_class.superclass->core_class.realize)
	(gw, valuemaskp, attr);

    /*
     * build up the pixmaps that we'll put into the image
     */
    if (w->mailbox.full.bitmap == None) {
	w->mailbox.full.bitmap = 
	  XCreateBitmapFromData (dpy, w->core.window, (char *) mailfull_bits,
				 mailfull_width, mailfull_height);
    }
    if (w->mailbox.empty.bitmap == None) {
	w->mailbox.empty.bitmap =
	  XCreateBitmapFromData (dpy, w->core.window, (char *) mailempty_bits,
				 mailempty_width, mailempty_height);
    }

    w->mailbox.empty.pixmap = make_pixmap (dpy, w, w->mailbox.empty.bitmap,
					   depth, False,
					   &w->mailbox.empty.width,
					   &w->mailbox.empty.height);
    w->mailbox.full.pixmap = make_pixmap (dpy, w, w->mailbox.full.bitmap,
					  depth, w->mailbox.flipit,
					  &w->mailbox.full.width,
					  &w->mailbox.full.height);
			 
    if (w->mailbox.empty.mask == None && w->mailbox.full.mask == None)
      w->mailbox.shapeit = False;

    w->mailbox.interval_id = 
	XtAppAddTimeOut (XtWidgetToApplicationContext((Widget) w),
			 w->mailbox.update * 1000, clock_tic, (XtPointer) w);

    w->mailbox.shape_cache.mask = None;

    check_mailbox (w, TRUE, FALSE);

    return;
}


static void Destroy (gw)
    Widget gw;
{
    MailboxWidget w = (MailboxWidget) gw;
    Display *dpy = XtDisplay (gw);

    XtFree (w->mailbox.filename);
    if (w->mailbox.interval_id) XtRemoveTimeOut (w->mailbox.interval_id);
    XtReleaseGC(gw, w->mailbox.gc);
#define freepix(p) if (p) XFreePixmap (dpy, p)
    freepix (w->mailbox.full.bitmap);		/* until cvter does ref cnt */
    freepix (w->mailbox.full.mask);		/* until cvter does ref cnt */
    freepix (w->mailbox.full.pixmap);
    freepix (w->mailbox.empty.bitmap);		/* until cvter does ref cnt */
    freepix (w->mailbox.empty.mask);		/* until cvter does ref cnt */
    freepix (w->mailbox.empty.pixmap);
    freepix (w->mailbox.shape_cache.mask);
#undef freepix
    return;
}


static void Redisplay (gw, event, region)
    Widget gw;
    XEvent *event;
    Region region;
{
    MailboxWidget w = (MailboxWidget) gw;

    check_mailbox (w, TRUE, FALSE);
}


static void check_mailbox (w, force_redraw, reset)
    MailboxWidget w;
    Boolean force_redraw, reset;
{
    long mailboxsize = 0;
    Boolean readSinceLastWrite = FALSE;

    if (w->mailbox.check_command != NULL) {
	waitType wait_status;
	int	check_status;
#ifdef INTWAITTYPE
	wait_status = system(w->mailbox.check_command);
#else
	wait_status.w_status = system(w->mailbox.check_command);
#endif
	check_status = waitCode(wait_status);

	/* error in sh checkCommand execution */
	if (waitSig(wait_status))
	    check_status = 2;		/* act as if there is no mail */

	switch (check_status) {
	  case 0:
	    mailboxsize = w->mailbox.last_size + 1;
	    break;
	  case 2:
	    mailboxsize = 0;
	    break;
	  default:	/* treat everything else as no change */
	    	        /* case 1 is no change */
	    mailboxsize = w->mailbox.last_size;
	}
    } else {
	struct stat st;
	if (stat (w->mailbox.filename, &st) == 0) {
	    mailboxsize = st.st_size;
	    readSinceLastWrite = (st.st_atime > st.st_mtime);
	}
    }

    /*
     * Now check for changes.  If reset is set then we want to pretent that
     * there is no mail.  If the mailbox is empty then we want to turn off
     * the flag.  Otherwise if the mailbox has changed size then we want to
     * put the flag up, unless the mailbox has been read since the last 
     * write.
     *
     * The cases are:
     *    o  forced reset by user                        DOWN
     *    o  no mailbox or empty (zero-sized) mailbox    DOWN
     *    o  if read after most recent write 		 DOWN
     *    o  same size as last time                      no change
     *    o  bigger than last time                       UP
     *    o  smaller than last time but non-zero         UP
     *
     * The last two cases can be expressed as different from last
     * time and non-zero.
     */

    if (reset) {			/* forced reset */
	w->mailbox.flag_up = FALSE;
	force_redraw = TRUE;
    } else if (mailboxsize == 0) {	/* no mailbox or empty */
	w->mailbox.flag_up = FALSE;
	if (w->mailbox.last_size > 0) force_redraw = TRUE;  /* if change */
    } else if (readSinceLastWrite) { 	/* only when checkCommand is NULL */
	/* mailbox has been read after most recent write */
	if (w->mailbox.flag_up) {
	    w->mailbox.flag_up = FALSE;
	    force_redraw = TRUE;
	}
    } else if (mailboxsize != w->mailbox.last_size) {  /* different size */
	if (!w->mailbox.once_only || !w->mailbox.flag_up)
	    beep(w); 
	if (!w->mailbox.flag_up)
	    force_redraw = w->mailbox.flag_up = TRUE;
    } 

    w->mailbox.last_size = mailboxsize;
    if (force_redraw) redraw_mailbox (w);
    return;
}

/*
 * get user name for building mailbox
 */

static void GetMailFile (w)
    MailboxWidget w;
{
    char *username;
    char *mailpath;
#ifdef WIN32
    if (!(username = getenv("USERNAME"))) {
	fprintf (stderr, "%s:  unable to find a username for you.\n",
		 "Mailbox widget");
	CloseDown (w, 1);
    }
#else
    char *getlogin();

    username = getlogin ();
    if (!username) {
	struct passwd *pw = getpwuid (getuid ());

	if (!pw) {
	    fprintf (stderr, "%s:  unable to find a username for you.\n",
		     "Mailbox widget");
	    CloseDown (w, 1);
	}
	username = pw->pw_name;
    }
#endif
    if ((mailpath = getenv("MAIL"))) {
	w->mailbox.filename = (String) XtMalloc (strlen (mailpath) + 1);
	strcpy (w->mailbox.filename, mailpath);
    } else {
	w->mailbox.filename = (String) XtMalloc (strlen (MAILBOX_DIRECTORY) + 1
						 + strlen (username) + 1);
	strcpy (w->mailbox.filename, MAILBOX_DIRECTORY);
	strcat (w->mailbox.filename, "/");
	strcat (w->mailbox.filename, username);
    }
    return;
}

static void CloseDown (w, status)
    MailboxWidget w;
    int status;
{
    Display *dpy = XtDisplay (w);

    XtDestroyWidget ((Widget)w);
    XCloseDisplay (dpy);
    exit (status);
}


/* ARGSUSED */
static Boolean SetValues (gcurrent, grequest, gnew, args, num_args)
    Widget gcurrent, grequest, gnew;
    ArgList args;
    Cardinal *num_args;
{
    MailboxWidget current = (MailboxWidget) gcurrent;
    MailboxWidget new = (MailboxWidget) gnew;
    Boolean redisplay = FALSE;

    if (current->mailbox.update != new->mailbox.update) {
	if (current->mailbox.interval_id) 
	  XtRemoveTimeOut (current->mailbox.interval_id);
	new->mailbox.interval_id =
	    XtAppAddTimeOut (XtWidgetToApplicationContext(gnew),
			     new->mailbox.update * 1000, clock_tic,
			     (XtPointer) gnew);
    }

    if (current->mailbox.foreground_pixel != new->mailbox.foreground_pixel ||
	current->core.background_pixel != new->core.background_pixel) {
	XtReleaseGC (gcurrent, current->mailbox.gc);
	new->mailbox.gc = get_mailbox_gc (new);
	redisplay = TRUE;
    }

    return (redisplay);
}


/*
 * drawing code
 */

static void redraw_mailbox (w)
    MailboxWidget w;
{
    register Display *dpy = XtDisplay (w);
    register Window win = XtWindow (w);
    register int x, y;
    GC gc = w->mailbox.gc;
    Pixel back = w->core.background_pixel;
    struct _mbimage *im;

    /* center the picture in the window */

    if (w->mailbox.flag_up) {		/* paint the "up" position */
	im = &w->mailbox.full;
	if (w->mailbox.flipit) back = w->mailbox.foreground_pixel;
    } else {				/* paint the "down" position */
	im = &w->mailbox.empty;
    }
    x = (((int)w->core.width) - im->width) / 2;
    y = (((int)w->core.height) - im->height) / 2;

    XSetWindowBackground (dpy, win, back);
    XClearWindow (dpy, win);
    XCopyArea (dpy, im->pixmap, win, gc, 0, 0, im->width, im->height, x, y);

    /*
     * XXX - temporary hack; walk up widget tree to find top most parent (which
     * will be a shell) and mash it to have our shape.  This will be replaced
     * by a special shell widget.
     */
    if (w->mailbox.shapeit) {
	Widget parent;

	for (parent = (Widget) w; XtParent(parent);
	     parent = XtParent(parent)) {
	    x += parent->core.x + parent->core.border_width;
	    y += parent->core.y + parent->core.border_width;
	}

	if (im->mask != w->mailbox.shape_cache.mask ||
	    x != w->mailbox.shape_cache.x || y != w->mailbox.shape_cache.y) {
	    XShapeCombineMask (XtDisplay(parent), XtWindow(parent),
			       ShapeBounding, x, y, im->mask, ShapeSet);
	    w->mailbox.shape_cache.mask = im->mask;
	    w->mailbox.shape_cache.x = x;
	    w->mailbox.shape_cache.y = y;
	}
    }

    return;
}


static void beep (w)
    MailboxWidget w;
{
    XBell (XtDisplay (w), w->mailbox.volume);
    return;
}
