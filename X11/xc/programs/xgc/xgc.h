/*
** xgc
**
** xgc.h
*/
/* $XFree86: xc/programs/xgc/xgc.h,v 1.3 2000/02/17 14:00:38 dawes Exp $ */

#include "constants.h"

typedef struct {
  Display  *dpy;		/* the display! */
  Screen   *scr;		/* the screen! */
  Window    win;		/* the window the test runs in */
  GC        gc;			/* the GC! */
  GC        miscgc;		/* used for doing stuff when we don't want
				   to change the normal GC */
  XGCValues gcv;		/* a separate copy of what's in the GC,
				   since we're not allowed to look in it */
  Pixmap    tile;		/* what we tile with */
  Pixmap    stipple;		/* what we stipple with */
  XImage   *image;		/* image for GetImage & PutImage */
  int       test;		/* which test is being run */
  float     percent;		/* percentage of test to run */
  Pixel     foreground;
  Pixel     background;
  char      *fontname;
} XStuff;                       /* All the stuff that only X needs to
                                   know about */

typedef struct {
  char name[40];  		/* name as it will appear on the screen */
  char text[40];	       	/* Xgc command it translates to */
  int num_commands;		/* number of command buttons inside it */
  int columns;			/* how many columns of command buttons; if
				   0, then there's only one row */
  struct {
    char name[40];		/* name as it will appear on the screen */
    char text[40];   		/* Xgc command it translates to */
  } command_data[MAXCHOICES];
} ChoiceStuff;			/* All the info needed to deal with a 
				   choice widget */
typedef struct {
  const char *name;
  const char *text;
  int code;
} XgcData;

typedef struct {
  struct {
    const char *name;
    const char *text;
    int   num_toggles;
    int   columns;
  } choice;
  XgcData *data;
} XgcStuff;

typedef struct {
  Widget label;
  int size;
  WidgetList widgets;
} ChoiceDesc;

/************/
