/*
** xgc
**
** xgc.h
*/
/* $XFree86: xc/programs/xgc/xgc.h,v 1.6 2003/09/13 21:33:11 dawes Exp $ */

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

#ifndef HAS_SNPRINTF
#include <X11/Xmu/SysUtil.h>
#define snprintf XmuSnprintf
#endif

/************/

extern XStuff X;
extern Widget topform;
extern XtAppContext appcontext;

extern XgcStuff TestStuff;
extern XgcStuff FunctionStuff;
extern XgcStuff LinestyleStuff;
extern XgcStuff CapstyleStuff;
extern XgcStuff JoinstyleStuff;
extern XgcStuff FillstyleStuff;
extern XgcStuff FillruleStuff;
extern XgcStuff ArcmodeStuff;

extern ChoiceDesc *GCdescs[];
extern ChoiceDesc *testchoicedesc;
extern Widget test;
extern Widget GCform;
extern Widget foregroundtext;
extern Widget backgroundtext;
extern Widget linewidthtext;
extern Widget fonttext;
extern Widget dashlistchoice;
extern Widget planemaskchoice;
extern Widget testchoiceform;

extern Widget result;

extern Boolean recording;
extern Widget filename_text_widget, recordbutton;

extern int fildes[2];
extern FILE *outend;
extern FILE *yyin;

/************/

extern void change_percent(int, Boolean);
extern void change_test(int, Boolean);
extern void change_text(Widget, String);
extern void choose_defaults(ChoiceDesc *[], int);
extern void circle_line_test(int, int);
extern void close_file_if_recording(void);
extern void copyarea_test(void);
extern void copyplane_test(void);
extern ChoiceDesc * create_choice(Widget, XgcStuff *);
extern void create_dashlist_choice(Widget);
extern void create_planemask_choice(Widget);
extern void create_testfrac_choice(Widget);
extern Widget create_text_choice(Widget, int, int, int);
extern void fillpolygon_test(void);
extern void GC_change_arcmode(int, Boolean);
extern void GC_change_background(unsigned long, Boolean);
extern void GC_change_capstyle(int, Boolean);
extern void GC_change_dashlist(int, Boolean);
extern void GC_change_fillrule(int, Boolean);
extern void GC_change_fillstyle(int, Boolean);
extern void GC_change_font(char *, Boolean);
extern void GC_change_foreground(unsigned long, Boolean);
extern void GC_change_function(int, Boolean);
extern void GC_change_joinstyle(int, Boolean);
extern void GC_change_linestyle(int, Boolean);
extern void GC_change_linewidth(int, Boolean);
extern void GC_change_planemask(unsigned long, Boolean);
extern void genericarc_test(Boolean);
extern void genericrectangle_test(Boolean);
extern void get_filename(void (*)(void), void (*)(void));
extern void imagetext16_test(void);
extern void imagetext8_test(void);
extern void interpret(const char *);
extern void line_up_labels(ChoiceDesc *[], int);
extern void polyarc_test(void);
extern void polyfillarc_test(void);
extern void polyfillrectangle_test(void);
extern void polyline_test(void);
extern void polypoint_test(void);
extern void polyrectangle_test(void);
extern void polysegment_test(void);
extern void polytext16_test(void);
extern void polytext8_test(void);
extern void print_if_recording(const char *);
extern void putimage_test(void);
extern void read_from_keyboard(void);
extern void run_test(void);
extern void select_button(ChoiceDesc *, int);
extern void set_text(Widget, char *);
extern void show_result(char *);
extern void start_playback(void);
extern void toggle_recordbutton(Widget, caddr_t, caddr_t);
extern void update_dashlist(int);
extern void update_planemask(long);
extern void update_slider(int);
extern void yyerror(const char *);
extern int yylex(void);
extern int yyparse(void);
extern void yyrestart(FILE *);
extern int yywrap(void);


