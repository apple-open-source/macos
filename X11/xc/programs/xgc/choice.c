/*
** xgc
**
** choice.c
**
** All the generic stuff for dealing with choice widgets.
*/

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Toggle.h>

#include "xgc.h"

static void print_text_to_buffer(
#if NeedFunctionPrototypes
     Widget, caddr_t, caddr_t
#endif
);
extern void interpret();

extern XStuff X;

/* create_choice(w,info)
** ---------------------
** What a choice widget is:  A collection of toggle buttons placed inside
** a form widget.  Exactly one of these toggle buttons can be "on" at
** any given time;  the rest are "off".  "On" toggle buttons have
** the foreground and background colors reversed.
** Also, specifically because it comes in handy in xgc, choosing one
** of the buttons causes a string associated with it to be printed out
** (and interpreted).  Half of the string is global to the whole form
** and the other half is local to each button.
**
** For example, pressing the "xor" button in the "function" form would
** cause xgc to interpret the string "function xor", thus changing the
** function in the GC to xor.
**
** There's also a label widget to the left of that mess, with an
** incredibly descriptive title.
**
** create_choice() makes one.
**
** w is the form widget (already created) into which we will place the
** toggle buttons.  info contains lots of useful information, such
** as the names of the buttons and their strings (see xgc.h).
*/

ChoiceDesc *
create_choice(w,info)
     Widget w;
     XgcStuff *info;
{
  ChoiceDesc *choice;		/* What we will return.  Contains
				** Widget ID's of the label and toggles. */
  int i;			/* Counter */
  char *text;			/* Text to be interpreted when the
				** toggle widget is selected. */

  /* ArgList for the label widget */
  static Arg labelargs[] = {	
    {XtNborderWidth,  (XtArgVal) 0}, 
    {XtNjustify,      (XtArgVal) XtJustifyRight},
    {XtNvertDistance, (XtArgVal) 4}
  };

  /* ArgList for the toggle widgets */
  static Arg toggleargs[] = {
    {XtNfromHoriz,     (XtArgVal) NULL},
    {XtNfromVert,      (XtArgVal) NULL},
    {XtNhorizDistance, (XtArgVal) 4},
    {XtNvertDistance,  (XtArgVal) 4},
    {XtNradioGroup,    (XtArgVal) NULL},
    {XtNcallback,      (XtArgVal) NULL}
  };

  /* Callback list for the toggle widgets */
  static XtCallbackRec callbacklist[] = {
    {(XtCallbackProc)  print_text_to_buffer, NULL},
    {NULL,                                   NULL}
  };

  /* Allocate space for the widgets and initialize choice */
  choice = (ChoiceDesc *) XtMalloc(sizeof(ChoiceDesc));
  choice->widgets = (WidgetList) XtMalloc(sizeof(Widget) * 
					  info->choice.num_toggles);
  choice->size = info->choice.num_toggles;
  choice->label = XtCreateManagedWidget(info->choice.name,labelWidgetClass,w,
					labelargs,XtNumber(labelargs));

  /* set up the toggle widgets */
  toggleargs[5].value = (XtArgVal) callbacklist;
  for (i = 0; i < info->choice.num_toggles; ++i) {
    if (i == 0) {
      /* the upper left toggle; put it next to the label
       and don't worry about radio groups */
      toggleargs[0].value = (XtArgVal) choice->label;
      toggleargs[1].value = (XtArgVal) NULL;
      toggleargs[2].value = (XtArgVal) 10;
      toggleargs[3].value = (XtArgVal) 4;
      toggleargs[4].value = (XtArgVal) NULL;
    }
    else {
      toggleargs[4].value = (XtArgVal) choice->widgets[0];
				/* are we starting a new row? */
      if (info->choice.columns > 0 && 
	  i > 1 && 
	  (i % (info->choice.columns) == 0)) {
	toggleargs[0].value = (XtArgVal) choice->label;
	/* under the appropriate toggle */
	toggleargs[1].value = (XtArgVal) choice->widgets[i - info->choice.columns];
	toggleargs[2].value = (XtArgVal) 10;
	toggleargs[3].value = (XtArgVal) 4;
      }
      else {			/* we're in the middle of a row */
	/* to the right of the previous toggle */
	toggleargs[0].value = (XtArgVal) choice->widgets[i - 1];
	toggleargs[1].value = (XtArgVal) NULL;
	toggleargs[2].value = (XtArgVal) -1; /* overlapping slightly */
	toggleargs[3].value = (XtArgVal) 4;
      }

      if (info->choice.columns > 0 &&
	  i >= info->choice.columns) {
	/* correct vertical spacing */
	toggleargs[1].value = (XtArgVal) choice->widgets[i - info->choice.columns];
	toggleargs[3].value = (XtArgVal) -1;
      }
    }

    /* Put the correct stuff in the text field */
    text = (char *) XtMalloc((unsigned) (strlen(info->choice.text) +
					 strlen((info->data)[i].text) + 3));
    strcpy(text, info->choice.text);
    strcat(text, " ");
    strcat(text, (info->data)[i].text);
    strcat(text, "\n");
    callbacklist[0].closure = (caddr_t) text;
    
    /* Create it finally */
    choice->widgets[i] = XtCreateManagedWidget((info->data[i]).name, 
					       toggleWidgetClass,
					       w,
					       toggleargs,
					       XtNumber(toggleargs));
  }
  
  /* The toggle widgets have all been created;
  ** now make the all the same width if that's
  ** what we want to do.                    */

  if (info->choice.columns > 0) {
    Dimension maxwidth = 0;	/* maximum width we've found */
    Dimension width;		/* width of the current widget */
    static Arg args[] = {	/* for getting and setting the width */
      {XtNwidth,    (XtArgVal) NULL}
    };

    args[0].value = (XtArgVal) &width;

    /* Find the maximum width of any toggle widget */
    for (i = 0; i < info->choice.num_toggles; ++i) {
      XtGetValues(choice->widgets[i],args,1);
      maxwidth = max(maxwidth,width);
    }

    /* Now set them all to that width */
    args[0].value = (XtArgVal) maxwidth;
    for (i = 0; i < info->choice.num_toggles; ++i)
      XtSetValues(choice->widgets[i],args,1);
  }

  /* return the list of toggles that were just created */
  return (choice);
}



/* select_button(choice,togglenum)
** -------------------------------
** "Selects" the togglenumth toggle widget in the choice layout
** represented by choice.  It simply turns the widget on, as if the
** user had selected it, without calling any callbacks.  It's used
** to give feedback when reading from a script.
*/

void
select_button(choice,togglenum)
     ChoiceDesc *choice;
     int togglenum;
{
  static Arg toggleargs[] = {
    {XtNstate,   (XtArgVal) True}
  };

  XtSetValues(choice->widgets[togglenum],toggleargs,XtNumber(toggleargs));
}

/* line_up_labels(descs,numdescs)
** ------------------------------
** descs represents a bunch of choice layouts (numdescs is the size of
** descs).  This function sets each label in descs to the same width,
** thus making them line up nicely since they're all on the left margin.
*/

void
line_up_labels(descs,numdescs)
     ChoiceDesc *descs[];
     int numdescs;
{
  int i;			/* counter */
  Dimension width;		/* current width */
  Dimension maxwidth = (Dimension) 0; /* max width found */

  static Arg widthargs[] = {
    {XtNwidth,     (XtArgVal) NULL }
  };

  widthargs[0].value = (XtArgVal) &width;

  /* Find the maximum width */
  for (i = 0; i < numdescs; ++i) {
    XtGetValues(descs[i]->label, widthargs, XtNumber(widthargs));
    maxwidth = max(maxwidth,width);
  }

  /* Set all labels to that width */
  widthargs[0].value = (XtArgVal) maxwidth;
  for (i = 0; i < numdescs; ++i) {
    XtSetValues(descs[i]->label, widthargs, XtNumber(widthargs));
  }
}

/* choose_defaults(descs,numdescs)
** -------------------------------
** descs represents a bunch of choice layouts (numdescs is the size of
** descs).  This function goes through all of descs and selects the
** appropriate toggle widget for each one.  This includes calling
** the callbacks associated with that widget.
**
** This function ends up initializing both the screen and the GC, and
** ensures that they are consistent.
*/

void
choose_defaults(descs,numdescs)
     ChoiceDesc *descs[];
     int numdescs;
{
  int i;			/* which choice layout */
  int j;			/* which toggle within it */

  for (i = 0; i < numdescs; ++i) {
    j = 0;
    if (i == 0)
      j = 3;
    select_button(descs[i],j);
    XtCallCallbacks(descs[i]->widgets[j], XtNcallback, (caddr_t) NULL);
  }
}


/* print_text_to_buffer(w,closure,call_data)
** -----------------------------------------
** This is also in the list of callbacks for the toggle buttons in a
** choice widget.  It sends the string contained in closure (which
** was set way back in create_choice()) over to interpret(), which
** decides what to do with it.
*/

/*ARGSUSED*/
static void
print_text_to_buffer(w,closure,call_data)
     Widget  w;
     caddr_t closure;           /* contains the string */
     caddr_t call_data;
{
  interpret((char *) closure);          /* Gee, that was easy */
}
