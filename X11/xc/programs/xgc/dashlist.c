/*
** dashlist.c
**
** How to make a widget to choose a dashlist.
**
** NOTE: This file uses static variables.  Therefore, trying to use these
**       functions to create more than one of these dashlist choice things
**       will fail in a big way.
*/
/* $XFree86: xc/programs/xgc/dashlist.c,v 1.3 2001/07/29 21:23:21 tsi Exp $ */

#include <stdio.h>
#include <X11/Xos.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Toggle.h>
#include "xgc.h"

static void change_dashlist(
#if NeedFunctionPrototypes
     Widget, caddr_t, caddr_t
#endif
);
extern void interpret();

extern XStuff X;

static short dashlist = 240;	/* in binary, becomes the dashlist
				   (240 = XXXX____) */
static Widget *dashes;	        /* the toggle widgets */

/* create_dashlist_choice(w)
** -------------------------
** Inside w (a form widget), creates a bunch of little toggle buttons
** in a row, representing the dash list.  There's also a label so
** the user knows what it is.
*/

void
create_dashlist_choice(w)
     Widget w;
{
  /* callback list for the toggle widgets */
  static XtCallbackRec callbacklist[] = {
    {(XtCallbackProc) change_dashlist, NULL},
    {NULL,                             NULL}
  };

  /* ArgList for the label */
  static Arg labelargs[] = {
    {XtNborderWidth,  (XtArgVal) 0},
    {XtNjustify,      (XtArgVal) XtJustifyRight},
    {XtNvertDistance, (XtArgVal) 4}
  };

  /* ArgList for the toggles */
  static Arg dashargs[] = {
    {XtNcallback,           (XtArgVal) NULL},
    {XtNhorizDistance,      (XtArgVal) NULL},
    {XtNfromHoriz,          (XtArgVal) NULL},
    {XtNwidth,              (XtArgVal) 10},
    {XtNheight,             (XtArgVal) 10},
    {XtNhighlightThickness, (XtArgVal) 1},
    {XtNstate,              (XtArgVal) False},
    {XtNlabel,              (XtArgVal) ""}
  };

  static Widget label;		/* the label, of course */
  static int *dashinfo;		/* contains integers saying which bit
				   a particular button is; sent to
				   change_dashlist to tell it which
				   bit got changed */
  int i;			/* counter */

  char name[11];

  /* allocate space for stuff that we don't know the size of yet */
  dashes = (Widget *) malloc(DASHLENGTH * sizeof(Widget));
  dashinfo = (int *) malloc(DASHLENGTH * sizeof(int));

  /* make the label widget */
  label = XtCreateManagedWidget("dashlist",labelWidgetClass,w,
				labelargs,XtNumber(labelargs));

  dashargs[0].value = (XtArgVal) callbacklist;

  for (i=0;i<DASHLENGTH;++i) {	/* go through all the buttons */
    if (i==0) {			/* offset the first one from the label */
      dashargs[1].value = (XtArgVal) 10;
      dashargs[2].value = (XtArgVal) label;
    }
    else {			/* put it directly to the right of the
				   last one, no space in between */
      dashargs[1].value = (XtArgVal) -1;
      dashargs[2].value = (XtArgVal) dashes[i-1];
    }

    /* set its original state depending on the state of that
    ** bit of the dashlist */

    if (dashlist&1<<i)
      dashargs[6].value = (XtArgVal) True;
    else
      dashargs[6].value = (XtArgVal) False;

    sprintf(name,"dashlist%d",i);

    dashinfo[i] = i;		/* which bit we're on; this is needed
				   in change_dashlist (the callback) */
    callbacklist[0].closure = (caddr_t) &dashinfo[i];

    dashes[i] = XtCreateManagedWidget(name,toggleWidgetClass,w,
				  dashargs,XtNumber(dashargs));
  }
}

/* change_dashlist(w,closure,call_data)
** ------------------------------------
** This function is called when the user toggles a toggle widget.  It
** makes the appropriate change to the dashlist and sends it off
** to interpret().
** Funny args are because it's a callback.
*/

/*ARGSUSED*/
static void
change_dashlist(w,closure,call_data)
     Widget w;
     caddr_t closure;
     caddr_t call_data;
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

  /* modify the dashlist as appropriate. */
  if (on) {			
    dashlist |= 1<<num;		
  }
  else {			
    dashlist &= ~(1<<num);	
  }

  /* now tell interpret() about it */
  sprintf(buf,"dashlist %d\n",dashlist); 
  interpret(buf,FALSE);
}

/* update_dashlist(newdash)
** ------------------------
** Updates the display of the dashlist so that it corresponds to
** newdash.
*/

void
update_dashlist(newdash)
     int newdash;
{
  int i;			/* counter */
  static Arg dashargs[] = {	/* Arglist for setting toggle state */
    {XtNstate,   (XtArgVal) NULL}
  };

  /* first set the internal representation */
  dashlist = newdash;

  for (i = 0; i < DASHLENGTH; ++i) {
    if (newdash & 1<<i) 	/* if it's set, make it look that way */
      dashargs[0].value = (XtArgVal) True;
    else
      dashargs[0].value = (XtArgVal) False;

    XtSetValues(dashes[i],dashargs,XtNumber(dashargs));
  }
}
