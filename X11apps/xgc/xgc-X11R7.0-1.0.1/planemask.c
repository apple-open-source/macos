/*
** planemask.c
**
** How to make a widget to choose a planemask.
**
** NOTE: This file uses static variables.  Therefore, trying to use these
**       functions to create more than one of these planemask choice things
**       will fail in a big way.
*/
/* $XFree86: xc/programs/xgc/planemask.c,v 1.3 2000/02/17 14:00:37 dawes Exp $ */

#include <X11/Xos.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Toggle.h>
#include "xgc.h"

static void choose_plane(Widget, caddr_t, caddr_t);

static unsigned long planemask;	
static Widget *pm;

/* create_planemask_choice(w)
** -------------------------
** Inside w (a form widget), creates a bunch of little toggle buttons
** in a row, representing the planemask.  There's also a label so
** the user knows what it is.
*/

void
create_planemask_choice(Widget w)
{
  /* callback list for the toggle widgets */
  static XtCallbackRec callbacklist[] = {
    {(XtCallbackProc) choose_plane, NULL},
    {NULL,                          NULL}
  };

  /* ArgList for the label */
  static Arg labelargs[] = {
    {XtNborderWidth,     (XtArgVal) 0},
    {XtNjustify,         (XtArgVal) XtJustifyRight},
    {XtNvertDistance,    (XtArgVal) 4}
  };

  /* ArgList for the toggles */
  static Arg pmargs[] = {
    {XtNcallback,          (XtArgVal) NULL},
    {XtNhorizDistance,     (XtArgVal) NULL},
    {XtNfromHoriz,         (XtArgVal) NULL},
    {XtNwidth,             (XtArgVal) 10},
    {XtNheight,            (XtArgVal) 10},
    {XtNhighlightThickness,(XtArgVal) 1},
    {XtNstate,             (XtArgVal) True},
    {XtNlabel,             (XtArgVal) ""}
  };

  static Widget label;		/* the label, of course */
  static int *pminfo;		/* contains integers saying which bit
				   a particular button is; sent to
				   choose_plane to tell it which
				   bit got changed */

  int i, num_planes;

  char buf[40];
  char name[12];

  num_planes = PlanesOfScreen(X.scr);

  planemask = (1<<num_planes)-1;
  snprintf(buf, sizeof buf, "planemask %lu",planemask);
  interpret(buf);

  /* Allocate space for stuff that we don't know the size of yet */

  pm = (Widget *) malloc(num_planes * sizeof(Widget));
  pminfo = (int *) malloc(num_planes * sizeof(int));

  /* Make the label widget */

  label = XtCreateManagedWidget("planemask",labelWidgetClass,w,
				labelargs,XtNumber(labelargs));

  pmargs[0].value = (XtArgVal) callbacklist;

  for (i=0;i<num_planes;++i) {	/* go through all the buttons */
    if (i==0) {			/* offset the first one from the label */
      pmargs[1].value = (XtArgVal) 10;
      pmargs[2].value = (XtArgVal) label;
    }
    else {			/* put it directly to the right of the
				   last one, no space in between */
      pmargs[1].value = (XtArgVal) -1;
      pmargs[2].value = (XtArgVal) pm[i-1];
    }

    /* set its original state depending on the state of that bit
    ** of the planemask */

    if (planemask & 1<<i)
      pmargs[6].value = (XtArgVal) True;
    else
      pmargs[6].value = (XtArgVal) False;

    snprintf(name, sizeof name, "planemask%d",i);

    pminfo[i] = i;		/* which bit we're on; this is needed in
				   choose_plane (the callback) */
    callbacklist[0].closure = (caddr_t) &pminfo[i];

    pm[i] = XtCreateManagedWidget(name,toggleWidgetClass,w,
				  pmargs,XtNumber(pmargs));
  }
}

/* choose_plane(w,closure,call_data)
** ------------------------------------
** This function is called when the user toggles a toggle widget.  It
** makes the appropriate change to the planemask and sends it off
** to interpret().
** Funny args are because it's a callback.
*/

/*ARGSUSED*/
static void
choose_plane(Widget w, caddr_t closure, caddr_t call_data)
{
  int num;			/* what number button it is */
  Boolean on;			/* is it currently on or off? */

  char buf[80];			/* string to send to interpret() */

  static Arg args[] = {
    {XtNstate,    (XtArgVal) NULL}
  };

  /* set up ArgList so that 'on' will contain the state */
  args[0].value = (XtArgVal) &on;

  num = * (int *) closure;	/* we put it here back in the last function */
  XtGetValues(w,args,XtNumber(args));

  /* Modify the planemask appropriately */

  if (on)
    planemask |= 1<<num;
  else
    planemask &= ~(1<<num);

  (void) snprintf(buf, sizeof buf, "planemask %lu\n",planemask);
  interpret(buf);
}

/* update_planemask(mask)
** ----------------------
** Updates the display of the planemask so that it corresponds to mask.
*/

void
update_planemask(long mask)
{
  int i;			/* counter */
  static Arg maskargs[] = {	/* ArgList for setting toggle state */
    {XtNstate,     (XtArgVal) NULL}
  };

  /* First set the internal representation */

  planemask = mask;

  for (i = 0; i < PlanesOfScreen(X.scr); ++i) {
    if (planemask & 1<<i) {        /* if it's set, make it look that way */
      maskargs[0].value = (XtArgVal) True;
    }
    else {
      maskargs[0].value = (XtArgVal) False;
    }

    XtSetValues(pm[i],maskargs,XtNumber(maskargs));
  }
}
