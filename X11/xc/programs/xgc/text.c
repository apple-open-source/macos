/*
** xgc
**
** text.c
**
** How to make a text widget that returns a string when the cursor
** leaves its window.
*/
/* $XFree86: xc/programs/xgc/text.c,v 1.3 2000/02/17 14:00:37 dawes Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/AsciiText.h>
#include "xgc.h"

static void WriteText();
extern void interpret();

extern XStuff X;
extern XtAppContext appcontext;

/* the strings which are displayed on the screen, edited, and sent
   to interpret() */
static char textstrings[NUMTEXTWIDGETS][80];

static char oldtextstrings[NUMTEXTWIDGETS][80];

static const char *defaultstrings[NUMTEXTWIDGETS] = {"0","6x10","0","1"};

/* The labels displayed next to them */
static const char *labels[NUMTEXTWIDGETS] = {"Line Width","Font","Foreground",
					     "Background"};

/* the first half of what gets sent to interpret() */
static const char *names[NUMTEXTWIDGETS] = {"linewidth ","font ","foreground ",
					    "background "};

/* create_text_choice(w,type,length,width)
** ---------------------------------------
** Inside w (a form), creates an editable text widget of width width.  The
** user can enter a string of up to length characters.  type is one of
** the constants defined in xgc.h; it decides things like the label,
** what string will be displayed and edited, etc.  When the pointer leaves
** the widget, the widget does the appropriate thing with the text
** inside it; the user doesn't have to press an "enter" button or anything.
** Returns the text widget which the user will edit.
*/

Widget
create_text_choice(w,type,length,width)
     Widget w;
     int type;
     int length, width;
{
  char translationtable[600];	/* for adding the new action (calling
				   WriteText() when the pointer leaves) */

  static XtActionsRec actionTable[] = {	/* likewise */
    {"WriteText",  WriteText},
    {"Nothing",    NULL}
  };

  static Arg labelargs[] = {
    {XtNborderWidth,   (XtArgVal) 0},
    {XtNjustify,       (XtArgVal) XtJustifyRight}
  };

  static Arg textargs[] = {
    {XtNeditType,   (XtArgVal) XawtextEdit},
    {XtNstring,     (XtArgVal) NULL},
    {XtNlength,     (XtArgVal) NULL},
    {XtNwidth,      (XtArgVal) NULL},
    {XtNhorizDistance, (XtArgVal) 10},
    {XtNfromHoriz,  (XtArgVal) NULL},
    {XtNinsertPosition, (XtArgVal) NULL},
    {XtNuseStringInPlace, (XtArgVal) True}
  };

  static Widget text;		/* the text widget */
  static Widget label;		/* the label widget */

  /* Disable keys which would cause the cursor to go off the single
  ** line that we want to display.  If the pointer leaves the window,
  ** update the GC accordingly.  The integer passed to WriteText is
  ** so it knows what type of widget was just updated. */

  sprintf(translationtable,
     "<Leave>:      WriteText(%d)\n\
     Ctrl<Key>J:    Nothing()\n\
     Ctrl<Key>M:    Nothing()\n\
     <Key>Linefeed: Nothing()\n\
     <Key>Return:   Nothing()\n\
     Ctrl<Key>O:    Nothing()\n\
     Meta<Key>I:    Nothing()\n\
     Ctrl<Key>N:    Nothing()\n\
     Ctrl<Key>P:    Nothing()\n\
     Ctrl<Key>Z:    Nothing()\n\
     Meta<Key>Z:    Nothing()\n\
     Ctrl<Key>V:    Nothing()\n\
     Meta<Key>V:    Nothing()",type);

  /* label uses type to find out what its title is */
  label = XtCreateManagedWidget(labels[type],labelWidgetClass,w,
				labelargs,XtNumber(labelargs));
  
  /* text uses type to find out what its string is */
  switch (type) {
  case TForeground:
    sprintf(textstrings[type],"%d",(int) X.gcv.foreground);
    sprintf(oldtextstrings[type],"%d",(int) X.gcv.foreground);
    break;
  case TBackground:
    sprintf(textstrings[type],"%d",(int) X.gcv.background);
    sprintf(oldtextstrings[type],"%d",(int) X.gcv.background);
    break;
  default:
    strcpy(textstrings[type],defaultstrings[type]);
    strcpy(oldtextstrings[type],defaultstrings[type]);
  }
  textargs[1].value = (XtArgVal) textstrings[type];
  textargs[2].value = (XtArgVal) length;
  textargs[3].value = (XtArgVal) width;
  textargs[5].value = (XtArgVal) label;
  textargs[6].value = (XtArgVal) strlen(textstrings[type]);

  text = XtCreateManagedWidget("text", asciiTextWidgetClass,w,
			       textargs,XtNumber(textargs));

  /* Register the actions and translations */

  XtAppAddActions(appcontext,actionTable,XtNumber(actionTable));
  XtOverrideTranslations(text,XtParseTranslationTable(translationtable));

  return(text);
}

/* WriteText(w,event,params,num_params)
** ------------------------------------
** Makes an appropriate string and sends it off to interpret().
** It's an ActionProc, thus the funny arguments.
*/

/*ARGSUSED*/
static void
WriteText(w,event,params,num_params)
     Widget w;
     XEvent *event;
     String *params;		/* the type is in here */
     Cardinal *num_params;
{
  char mbuf[80];
  int type;			/* which string # to send */

  type = atoi(params[0]);
  if (strcmp(textstrings[type],oldtextstrings[type])) {
    strcpy(oldtextstrings[type],textstrings[type]);
    sprintf(mbuf,names[type]);	/* the right first half */
    strcat(mbuf,textstrings[type]); /* the right second half */
    strcat(mbuf,"\n");		/* the right newline */
    interpret(mbuf);		/* send it off */
  }
}

/* change_text(w,type,newtext)
** ------------------------
** Changes the text in the text widget w of type type to newtext.
*/

void
change_text(w,newtext)
     Widget w;
     String newtext;
{
  XawTextBlock text;		/* the new text */
  XawTextPosition first, last;	/* boundaries of the old text */
  String oldtext;		/* the old text */

  static Arg textargs[] = {
    {XtNstring, (XtArgVal) 0}
  };

  /* Initialize the XawTextBlock. */

  if (!newtext)
      newtext = "";
  text.firstPos = 0;
  text.length = strlen(newtext);
  text.ptr = newtext;
  text.format = FMT8BIT;

  /* Find the old text, so we can get its length, so we know how
  ** much of it to update. */

  textargs[0].value = (XtArgVal) &oldtext;
  XtGetValues(w,textargs,XtNumber(textargs));
  first = XawTextTopPosition(w);
  if (!oldtext)
      oldtext = "";
  last = (XawTextPosition) strlen(oldtext)+1;

  /* Replace it with the new text. */

  XawTextReplace(w, first, last, &text);
}

