/*
** xgc
**
** record.c
*/
/* $XFree86: xc/programs/xgc/record.c,v 1.3 2000/02/17 14:00:37 dawes Exp $ */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Shell.h>
#include <X11/Xaw/AsciiText.h>
#include <stdio.h>

#include "xgc.h"

static void start_recording();
static void stop_recording();
static void print_out_gc_values();
static void chose_playback_filename();
static void cancel_playback();
extern void get_filename();
extern void yyparse();

extern XgcStuff TestStuff;
extern XgcStuff FunctionStuff;
extern XgcStuff LinestyleStuff;
extern XgcStuff CapstyleStuff;
extern XgcStuff JoinstyleStuff;
extern XgcStuff FillstyleStuff;
extern XgcStuff FillruleStuff;
extern XgcStuff ArcmodeStuff;

extern XStuff X;
extern Boolean recording;
extern Widget filename_text_widget, recordbutton;

static void cancel_record();
static void done_choosing_filename();

FILE *recordfile;		/* the file we're recording to */
FILE *playbackfile;		/* the file we're playing back from */
extern FILE *yyin;		/* for yyparse */

/* toggle_recordbutton(w,closure,call_data)
** ----------------------------------------
** This function is called when the user presses the "Record"
** command button.  If we're not recording, we start; if we are,
** we stop.  Also change the label to reflect the change in the
** function of the button.
*/

/*ARGSUSED*/
void
toggle_recordbutton(w,closure,call_data)
     Widget w;
     caddr_t closure;
     caddr_t call_data;
{
  /* ArgList for changing the label */
  static Arg recordargs[] = {
    {XtNlabel,        (XtArgVal) NULL}
  };
  
  char tmp[20];			/* new label */
  
  if (!recording) {
    start_recording();
  }
  else {
    recording = FALSE;
    stop_recording();
    sprintf(tmp,"Record");
    recordargs[0].value = (XtArgVal) tmp;
  }

  XtSetValues(recordbutton,recordargs,XtNumber(recordargs));
}

/* start_recording()
** -----------------
** Get the name of the file the user wants to record into, and
** start recording into it if he doesn't cancel.
*/

static void
start_recording() 
{
  get_filename(done_choosing_filename,cancel_record);
}

/* stop_recording()
** ----------------
** Close the output file.
*/

static void
stop_recording() 
{
  fclose(recordfile);
}

/* cancel_record()
** ---------------
** What to do if the if the user canceled recording, i.e. nothing.
*/

static void
cancel_record() 
{
}

/* done_choosing_filename()
** ------------------------
** What to do after the user's chosen a file.  Change the label on the
** command button, open the file, and dump the current contents of the
** GC into it.
*/

static void
done_choosing_filename() 
{
  static Arg recordargs[] = {
    {XtNlabel,        (XtArgVal) NULL},
    {XtNresize,       (XtArgVal) True}
  };
  Arg args[1];
  char tmp[20], *filename;

  XtSetArg(args[0], XtNstring, &filename);
  XtGetValues(filename_text_widget, args, (Cardinal) 1);

  if ((recordfile = fopen(filename,"w")) != NULL) {
    recording = TRUE;
    sprintf(tmp,"End Record");
    recordargs[0].value = (XtArgVal) tmp;
    XtSetValues(recordbutton,recordargs,XtNumber(recordargs));

    print_out_gc_values();
  }
}

/* print_if_recording(str)
** -----------------------
** If we're recording to a file, put str in it.
*/

void
print_if_recording(str)
     char *str;
{
  if (recording)
    fprintf(recordfile,"%s",str);
}

/* close_file_if_recording()
** -------------------------
** If we're recording, stop.
*/

void 
close_file_if_recording()
{
  if (recording)
    fclose(recordfile);
}

/* print_out_gc_values()
** ---------------------
** Dump the contents of the GC to the file, so that when the file gets
** played back, it will be correctly initialized.
*/

static void
print_out_gc_values()
{
  int i;
  for (i=0;i<NUM_TESTS;++i) {
    if ((TestStuff.data)[i].code == X.test) {
      fprintf(recordfile,"%s %s\n",
	      TestStuff.choice.text,(TestStuff.data)[i].text);
      break;
    }
  }
  for (i=0;i<NUM_FUNCTIONS;++i) {
    if ((FunctionStuff.data)[i].code == X.gcv.function) {
      fprintf(recordfile,"%s %s\n",
	      FunctionStuff.choice.text,(FunctionStuff.data)[i].text);
      break;
    }
  }
  for (i=0;i<NUM_LINESTYLES;++i) {
    if ((LinestyleStuff.data)[i].code == X.gcv.line_style) {
      fprintf(recordfile,"%s %s\n",
	      LinestyleStuff.choice.text,(LinestyleStuff.data)[i].text);
      break;
    }
  }
  for (i=0;i<NUM_CAPSTYLES;++i) {
    if ((CapstyleStuff.data)[i].code == X.gcv.cap_style) {
      fprintf(recordfile,"%s %s\n",
	      CapstyleStuff.choice.text,(CapstyleStuff.data)[i].text);
      break;
    }
  }
  for (i=0;i<NUM_JOINSTYLES;++i) {
    if ((JoinstyleStuff.data)[i].code == X.gcv.join_style) {
      fprintf(recordfile,"%s %s\n",
	      JoinstyleStuff.choice.text,(JoinstyleStuff.data)[i].text);
      break;
    }
  }
  for (i=0;i<NUM_FILLSTYLES;++i) {
    if ((FillstyleStuff.data)[i].code == X.gcv.fill_style) {
      fprintf(recordfile,"%s %s\n",
	      FillstyleStuff.choice.text,(FillstyleStuff.data)[i].text);
      break;
    }
  }
  for (i=0;i<NUM_FILLRULES;++i) {
    if ((FillruleStuff.data)[i].code == X.gcv.fill_rule) {
      fprintf(recordfile,"%s %s\n",
	      FillruleStuff.choice.text,(FillruleStuff.data)[i].text);
      break;
    }
  }
  for (i=0;i<NUM_ARCMODES;++i) {
    if ((ArcmodeStuff.data)[i].code == X.gcv.arc_mode) {
      fprintf(recordfile,"%s %s\n",
	      ArcmodeStuff.choice.text,(ArcmodeStuff.data)[i].text);
      break;
    }
  }
  fprintf(recordfile,"linewidth %d\n",X.gcv.line_width);
  fprintf(recordfile,"foreground %ld\n",X.gcv.foreground);
  fprintf(recordfile,"background %ld\n",X.gcv.background);
  fprintf(recordfile,"planemask %ld\n",X.gcv.plane_mask);
  fprintf(recordfile,"dashlist %d\n",X.gcv.dashes);
  fprintf(recordfile,"font %s\n",X.fontname);
}  

/********************************************/

/* start_playback()
** ----------------
** This gets called if the user wants to playback from a file.
** Get the file name and do the appropriate thing.
*/

void
start_playback()
{
  get_filename(chose_playback_filename,cancel_playback);
}

/* cancel_playback()
** -----------------
** What to do if the user canceled the playback request.
*/

static void
cancel_playback()
{
}

/* chose_playback_filename()
** -------------------------
** What to do once the user's selected a filename to playback.
** Play it back.
*/

static void
chose_playback_filename()
{
  Arg args[1];
  char *filename;

  XtSetArg(args[0], XtNstring, &filename);
  XtGetValues(filename_text_widget, args, (Cardinal) 1);

  if ((playbackfile = fopen(filename,"r")) != NULL) {
    yyin = playbackfile;
    yyparse();
  }
}

/* read_from_keyboard()
** --------------------
** Do a playback from the keyboard.
*/

void
read_from_keyboard()
{
  yyin = stdin;
  yyparse();
}
