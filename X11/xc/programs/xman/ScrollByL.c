/* $XConsortium: ScrollByL.c,v 1.30 94/04/17 20:43:46 rws Exp $ */
/*

Copyright (c) 1987, 1988  X Consortium

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
/* $XFree86: xc/programs/xman/ScrollByL.c,v 1.7 2003/05/27 22:27:07 tsi Exp $ */

#if !defined(lint) && !defined(SABER) && 0
  static char rcs_version[] = "$Athena: ScrollByL.c,v 4.5 88/12/19 13:46:04 kit Exp $";
#endif

#include <stdio.h>
#include <ctype.h>
#include <X11/Xos.h>
#include <stdlib.h>

#include <X11/IntrinsicP.h>
#include <sys/stat.h>		/* depends on IntrinsicP.h */
#include <X11/StringDefs.h>

#include <X11/Xaw/Scrollbar.h>
#include <X11/Xmu/Misc.h>

#include "ScrollByLP.h"

/* Default Translation Table */

static char defaultTranslations[] = 
  "<Key>f:      Page(Forward) \n\
   <Key>b:      Page(Back) \n\
   <Key>1:      Page(Line, 1) \n\
   <Key>2:      Page(Line, 2) \n\
   <Key>3:      Page(Line, 3) \n\
   <Key>4:      Page(Line, 4) \n\
   <Key>\\ :    Page(Forward)";

      
/****************************************************************
 *
 * ScrollByLine Resources
 *
 ****************************************************************/

#define Offset(field) XtOffset(ScrollByLineWidget, scroll.field)
#define CoreOffset(field) XtOffset(ScrollByLineWidget, core.field)

static XtResource resources[] = {
    {XtNwidth, XtCWidth, XtRDimension, sizeof(Dimension),
       CoreOffset(width), XtRImmediate, (caddr_t) 500},
    {XtNheight, XtCHeight, XtRDimension, sizeof(Dimension),
       CoreOffset(height), XtRImmediate, (caddr_t) 700},

    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
       Offset(foreground), XtRString, "XtDefaultForeground"},
    {XtNforceVert, XtCBoolean, XtRBoolean, sizeof(Boolean),
       Offset(force_vert), XtRImmediate, (caddr_t) FALSE},
    {XtNindent, XtCIndent, XtRDimension, sizeof(Dimension),
       Offset(indent), XtRImmediate, (caddr_t) 15},
    {XtNuseRight, XtCBoolean, XtRBoolean, sizeof(Boolean),
       Offset(use_right), XtRImmediate, (caddr_t) FALSE},
    {XtNmanualFontNormal, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
       Offset(normal_font), XtRString, MANPAGE_NORMAL},
    {XtNmanualFontBold, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
       Offset(bold_font), XtRString, MANPAGE_BOLD},
    {XtNmanualFontItalic, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
       Offset(italic_font), XtRString, MANPAGE_ITALIC},
    {XtNmanualFontSymbol, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
       Offset(symbol_font), XtRString, MANPAGE_SYMBOL},
    {XtNfile, XtCFile, XtRFile, sizeof(FILE *),
       Offset(file), XtRImmediate, (caddr_t) NULL},
};

#undef Offset
#undef CoreOffset

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

static Boolean ScrollVerticalText(Widget w, int new_line, Boolean force_redisp);
static void Layout(Widget w);
static void LoadFile(Widget w);
static void MoveAndClearText(Widget w, int old_y, int height, int new_y);
static void PaintText(Widget w, int y_loc, int height);
static void PrintText(Widget w, int start_line, int num_lines, int location);
static void SetThumbHeight(Widget w);
static void VerticalJump(Widget w, XtPointer junk, XtPointer percent_ptr);
static void VerticalScroll(Widget w, XtPointer client_data, XtPointer call_data);

/* semi - public functions. */

static void Realize(Widget w, Mask *valueMask, XSetWindowAttributes *attributes);
static void Initialize(Widget req, Widget new, ArgList args, Cardinal *num_args);
static void Destroy(Widget w);
static void Redisplay(Widget w, XEvent *event, Region region);
static void Page(Widget w, XEvent * event, String * params, Cardinal *num_params);
static Boolean SetValuesHook(Widget w, ArgList args, Cardinal *num_args);

static XtActionsRec actions[] = {
  { "Page",   Page},
};

#define superclass		(&simpleClassRec)
#define SuperClass		simpleWidgetClass

ScrollByLineClassRec scrollByLineClassRec = {
  {
/* core_class fields      */
    /* superclass         */    (WidgetClass) superclass,
    /* class_name         */    "ScrollByLine",
    /* widget_size        */    sizeof(ScrollByLineRec),
    /* class_initialize   */    NULL,
    /* class_part_init    */    NULL,
    /* class_inited       */	FALSE,
    /* initialize         */    Initialize,
    /* initialize_hook    */    NULL,
    /* realize            */    Realize,
    /* actions            */    actions,
    /* num_actions	  */	XtNumber(actions),
    /* resources          */    resources,
    /* num_resources      */    XtNumber(resources),
    /* xrm_class          */    NULLQUARK,
    /* compress_motion	  */	TRUE,
    /* compress_exposure  */	FALSE,
    /* compress_enterleave*/    TRUE,
    /* visible_interest   */    FALSE,
    /* destroy            */    Destroy,
    /* resize             */    Layout,
    /* expose             */    Redisplay,
    /* set_values         */    NULL,
    /* set_values_hook    */    SetValuesHook,
    /* set_values_almost  */    XtInheritSetValuesAlmost,
    /* get_values_hook    */    NULL,
    /* accept_focus       */    NULL,
    /* version            */    XtVersion,
    /* callback_private   */    NULL,
    /* tm_table           */    defaultTranslations,
    /* query_geometry	  */	XtInheritQueryGeometry,
    /* display_accelerator*/	XtInheritDisplayAccelerator,
    /* extension	  */	NULL,
  },
  { /* simple fields */
    /* change_sensitive		*/	XtInheritChangeSensitive
  }
};

WidgetClass scrollByLineWidgetClass = 
            (WidgetClass) &scrollByLineClassRec;


/****************************************************************
 *
 * Private Routines
 *
 ****************************************************************/

/*	Function Name: Layout
 *	Description: This function lays out the scroll widget.
 *	Arguments: w - the scroll widget.
 *                 key - a boolean: if true then resize the widget to the child
 *                                  if false the resize children to fit widget.
 *	Returns: TRUE if successful.
 */

static void
Layout(Widget w)
{    
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;
  Dimension width, height;
  Widget bar;
  Position bar_bw;

/* 
 * For now always show the bar.
 */

  bar = sblw->scroll.bar;
  height = sblw->core.height;
  width = sblw->core.width;
  bar_bw = bar->core.border_width;

  /* Move child and v_bar to correct location. */

  if (sblw->scroll.use_right) {
    XtMoveWidget(bar, width - (bar->core.width + bar_bw), - bar_bw);
    sblw->scroll.offset = 0;
  }
  else {
    XtMoveWidget(bar, - bar_bw, - bar_bw);
    sblw->scroll.offset = bar->core.width + bar_bw;
  }

  /* resize the scrollbar to be the correct height or width. */

  XtResizeWidget(bar, bar->core.width, height, bar->core.border_width);

  SetThumbHeight(w);
}

/* ARGSUSED */
static void 
GExpose(Widget w, XtPointer junk, XEvent *event, Boolean *cont)
{

/*
 * Graphics exposure events are not currently sent to exposure proc.
 */

  if (event->type == GraphicsExpose)
    Redisplay(w, event, NULL);

} /* ChildExpose */

/*
 * Repaint the widget's child Window Widget.
 */

/* ARGSUSED */
static void
Redisplay(Widget w, XEvent *event, Region region)
{
  int top, height;		/* the locations of the top and height
				   of the region that needs to be repainted. */
  
/*
 * This routine tells the client which sections of the window to 
 * repaint in his callback function which does the actual repainting.
 */

  if (event->type == Expose) {
    top = event->xexpose.y;
    height = event->xexpose.height;
  }
  else {
    top = event->xgraphicsexpose.y;
    height  = event->xgraphicsexpose.height;
  }
  
  PaintText(w, top, height);
} /* redisplay (expose) */

/*	Function Name: PaintText
 *	Description: paints the text at the give location for a given height.
 *	Arguments: w - the sbl widget.
 *                 y_loc, height - location and size of area to paint.
 *	Returns: none
 */

static void
PaintText(Widget w, int y_loc, int height)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;
  int start_line, num_lines, location;

  start_line = y_loc / sblw->scroll.font_height + sblw->scroll.line_pointer;

  if (start_line >= sblw->scroll.lines)
    return;
  
  num_lines = height / sblw->scroll.font_height + 1;

/*
 * Only integer arithmetic makes this possible. 
 */

  location =  y_loc / sblw->scroll.font_height * sblw->scroll.font_height;

  PrintText(w, start_line, num_lines, location);
} 

/*	Function Name: Page
 *	Description: This function pages the widget, by the amount it recieves
 *                   from the translation Manager.
 *	Arguments: w - the ScrollByLineWidget.
 *                 event - the event that caused this return.
 *                 params - the parameters passed to it.
 *                 num_params - the number of parameters.
 *	Returns: none.
 */

/* ARGSUSED */
static void 
Page(Widget w, XEvent * event, String * params, Cardinal *num_params)
{
   ScrollByLineWidget sblw = (ScrollByLineWidget) w;
   Widget bar = sblw->scroll.bar;

   if (*num_params < 1)
     return;
/*
 * If no scroll bar is visible then do not page, as the entire window is shown,
 * of scrolling has been turned off. 
 */

   if (bar == (Widget) NULL)
     return;

   switch ( params[0][0] ) {
   case 'f':
   case 'F':
     /* move one page forward */
     VerticalScroll(bar, NULL, (XtPointer)((long) bar->core.height));
     break;
   case 'b':
   case 'B':
     /* move one page backward */
     VerticalScroll(bar, NULL,  (XtPointer)(- (long) bar->core.height));
     break;
   case 'L':
   case 'l':
     /* move one line forward */
     VerticalScroll(bar, NULL, 
		(XtPointer)((long) atoi(params[1]) * sblw->scroll.font_height));
     break;
   default:
     return;
   }
}

/*	Function Name: CreateScrollbar
 *	Description: createst the scrollbar for us.
 *	Arguments: w - sblw widget.
 *	Returns: none.
 */

static void
CreateScrollbar(Widget w)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;
  Arg args[5];
  Cardinal num_args = 0;

  if (sblw->scroll.bar != NULL) 
    return;

  XtSetArg(args[num_args], XtNorientation, XtorientVertical); num_args++;
  
  sblw->scroll.bar = XtCreateWidget("scrollbar", scrollbarWidgetClass, w, 
				    args, num_args);
  XtAddCallback(sblw->scroll.bar, XtNscrollProc, VerticalScroll, NULL);
  XtAddCallback(sblw->scroll.bar, XtNjumpProc, VerticalJump, NULL); 
}

/*	Function Name: ScrollVerticalText
 *	Description: This accomplished the actual movement of the text.
 *	Arguments: w - the ScrollByLine Widget.
 *                 new_line - the new location for the line pointer
 *                 force_redisplay - should we force this window to get 
 *                                   redisplayed?
 *	Returns: True if the thumb needs to be moved.
 */

static Boolean
ScrollVerticalText(
Widget w,
int new_line,
Boolean force_redisp)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;
  int num_lines = (int)w->core.height / sblw->scroll.font_height + 1;
  int max_lines, old_line;
  Boolean move_thumb = FALSE;

/*
 * Do not let the window extend out of bounds.
 */

  if ( new_line < 0) {
    new_line = 0;
    move_thumb = TRUE;
  }
  else {
    max_lines = sblw->scroll.lines - (int)w->core.height / sblw->scroll.font_height;
    AssignMax(max_lines, 0);

    if ( new_line > max_lines ) {
      new_line = max_lines;
      move_thumb = TRUE;
    }
  }

/* 
 * If forced to redisplay then do a full redisplay and return.
 */

  old_line = sblw->scroll.line_pointer;	
  sblw->scroll.line_pointer = new_line;	/* Set current top of page. */

  if (force_redisp) 
    MoveAndClearText(w, 0, /* cause a full redisplay */ 0, 0);

  if (new_line == old_line)
    return(move_thumb);

/*
 * Scroll forward.
 */

  else if (new_line < old_line) {
    int lines_to_scroll = old_line - new_line;
    MoveAndClearText(w, 0, num_lines - lines_to_scroll, lines_to_scroll);
  }

/* 
 * Scroll back.
 */

  else {
    int lines_to_scroll = new_line - old_line;
    MoveAndClearText(w, lines_to_scroll, num_lines - lines_to_scroll, 0);
  }

  return(move_thumb);
}

/*	Function Name: MoveAndClearText
 *	Description: Blits as much text as it can and clear the
 *                   remaining area with generate exposures TRUE.
 *	Arguments: w - the sbl widget.
 *                 old_y - the old y position.
 *                 height - height of area to move.
 *                 new_y - new y position.
 *	Returns: none
 */
	   
static void
MoveAndClearText(Widget w, int old_y, int height, int new_y)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;
  int from_left = sblw->scroll.indent + sblw->scroll.offset - 1;
  int y_clear;

  old_y *= sblw->scroll.font_height;
  new_y *= sblw->scroll.font_height;
  height *= sblw->scroll.font_height;

/*
 * If we are already at the right location then do nothing.
 * (height == 0).
 *
 * If we have scrolled more than a screen height then just clear
 * the window.
 */

  if (height <= sblw->scroll.font_height) { /* avoid rounding errors. */
    XClearArea( XtDisplay(w), XtWindow(w), from_left, 0, 
	       (unsigned int) 0, (unsigned int) 0, FALSE);    
    PaintText(w, 0, (int) sblw->core.height);
    return;
  }

  if ((int)height + (int)old_y > (int)w->core.height)
    height = w->core.height - old_y;

  XCopyArea(XtDisplay(w), XtWindow(w), XtWindow(w), sblw->scroll.move_gc,
	    from_left, old_y, 
	    (unsigned int) w->core.width - from_left, (unsigned int) height,
	    from_left, new_y);

  if (old_y > new_y)
    height -= sblw->scroll.font_height/2;  /* clear 1/2 font of extra space,
					      to make sure we don't lose or
					      gain decenders. */
  else
    height -= sblw->scroll.font_height;  /* clear 1 font of extra space,
					    to make sure we don't overwrite
					    with a last line in buffer. */

  if (old_y > new_y)
    y_clear = height;
  else
    y_clear = 0;
  
/*
 * We cannot use generate exposures, since that may allow another move and
 * clear before the area get repainted, this would be bad.
 */

  XClearArea( XtDisplay(w), XtWindow(w), from_left, y_clear,
	     (unsigned int) 0, (unsigned int) (w->core.height - height),
	     FALSE);
  PaintText(w, (int) y_clear, (int) (w->core.height - height));
}

/*	Function Name: SetThumbHeight
 *	Description: Set the height of the thumb.
 *	Arguments: w - the sblw widget.
 *	Returns: none
 */

static void
SetThumbHeight(Widget w)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;
  float shown;

  if (sblw->scroll.bar == NULL)
    return;

  if (sblw->scroll.lines == 0) 
    shown = 1.0;
  else
    shown = (float) w->core.height / (float) (sblw->scroll.lines *
					      sblw->scroll.font_height);
  if (shown > 1.0)
    shown = 1.0;

  XawScrollbarSetThumb( sblw->scroll.bar, (float) -1, shown );
}

/*	Function Name: SetThumb
 *	Description: Set the thumb location.
 *	Arguments: w - the sblw.
 *	Returns: none
 */

static void
SetThumb(Widget w)
{
  float location;
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;

  if ( (sblw->scroll.lines == 0) || (sblw->scroll.bar == NULL) )
    return;

  location = (float) sblw->scroll.line_pointer / (float) sblw->scroll.lines; 
  XawScrollbarSetThumb( sblw->scroll.bar, location , (float) -1 );
}

/*	Function Name: VerticalJump.
 *	Description: This function moves the test
 *                   as the vertical scroll bar is moved.
 *	Arguments: w - the scrollbar widget.
 *                 junk - not used.
 *                 percent - the position of the scrollbar.
 *	Returns: none.
 */

/* ARGSUSED */
static void
VerticalJump(Widget w, XtPointer junk, XtPointer percent_ptr)
{
  float percent = *((float *) percent_ptr);
  int new_line;			/* The new location for the line pointer. */
  ScrollByLineWidget sblw = (ScrollByLineWidget) XtParent(w);

  new_line = (int) ((float) sblw->scroll.lines * percent);
  if (ScrollVerticalText( (Widget) sblw, new_line, FALSE))
    SetThumb((Widget) sblw);
}

/*	Function Name: VerticalScroll
 *	Description: This function moves the postition of the interior window
 *                   as the vertical scroll bar is moved.
 *	Arguments: w - the scrollbar widget.
 *                 junk - not used.
 *                 pos - the position of the cursor.
 *	Returns: none.
 */

/* ARGSUSED */
static void
VerticalScroll(Widget w, XtPointer client_data, XtPointer call_data)
{
  int pos = (int)(long) call_data;
  int new_line;			/* The new location for the line pointer. */
  ScrollByLineWidget sblw = (ScrollByLineWidget) XtParent(w);

  new_line = sblw->scroll.line_pointer + (pos / sblw->scroll.font_height);
  (void) ScrollVerticalText( (Widget) sblw, new_line, FALSE);
  SetThumb( (Widget) sblw);
}

int h_width;			/* main font width */

/* ARGSUSED */
static void 
Initialize(Widget req, Widget new, ArgList args, Cardinal *num_args)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) new;
  unsigned long figWidth;
  Atom atomNum;

  sblw->scroll.top_line = NULL;
  sblw->scroll.line_pointer = 0;
  LoadFile(new);
  sblw->scroll.bar = (Widget) NULL;

  sblw->scroll.font_height = (sblw->scroll.normal_font->max_bounds.ascent + 
			      sblw->scroll.normal_font->max_bounds.descent); 

  atomNum = XInternAtom(XtDisplay(req), "FIGURE_WIDTH", False);

  if (XGetFontProperty(sblw->scroll.normal_font, atomNum, &figWidth))
      h_width = figWidth;
  else
    h_width = XTextWidth(sblw->scroll.normal_font, "$", 1);



} /* Initialize. */

/*	Function Name: CreateGCs
 *	Description: Creates the graphics contexts that we need. 
 *	Arguments: w - the sblw.
 *	Returns: none
 */

static void
CreateGCs(Widget w)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;

  XtGCMask mask;
  XGCValues values;

  values.graphics_exposures = TRUE;
  sblw->scroll.move_gc = XtGetGC(w, GCGraphicsExposures, &values);

  mask = GCForeground | GCFont;
  values.foreground = sblw->scroll.foreground;
  
  values.font = sblw->scroll.normal_font->fid;
  sblw->scroll.normal_gc = XtGetGC(w, mask, &values);

  values.font = sblw->scroll.italic_font->fid;
  sblw->scroll.italic_gc = XtGetGC(w, mask, &values);

  values.font = sblw->scroll.bold_font->fid;
  sblw->scroll.bold_gc = XtGetGC(w, mask, &values);

  values.font = sblw->scroll.symbol_font->fid;
  sblw->scroll.symbol_gc = XtGetGC(w, mask, &values);
}

/*	Function Name: DestroyGCs
 *	Description: removes all gcs for this widget.
 *	Arguments: w - the widget.
 *	Returns: none
 */

static void
DestroyGCs(Widget w)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;

  XtReleaseGC(w, sblw->scroll.normal_gc);
  XtReleaseGC(w, sblw->scroll.bold_gc);
  XtReleaseGC(w, sblw->scroll.italic_gc);
  XtReleaseGC(w, sblw->scroll.move_gc);
}

static void
Realize(Widget w, Mask *valueMask, XSetWindowAttributes *attributes)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;

  CreateScrollbar(w);
  CreateGCs(w);
  Layout(w);
  (*SuperClass->core_class.realize) (w, valueMask, attributes);
  XtRealizeWidget(sblw->scroll.bar); /* realize scrollbar. */
  XtMapWidget(sblw->scroll.bar); /* map scrollbar. */

  XtAddEventHandler(w, 0, TRUE, GExpose, NULL); /* Get Graphics Exposures */
} /* Realize */

/*	Function Name: Destroy
 *	Description: Cleans up when we are killed.
 *	Arguments: w - the widget.
 *	Returns: none
 */

static void
Destroy(Widget w)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;

  if (sblw->scroll.bar != NULL)
    XtDestroyWidget(sblw->scroll.bar); /* Destroy scrollbar. */
  if (sblw->scroll.file != NULL)
    fclose(sblw->scroll.file);
  DestroyGCs(w);
}

/*
 *
 * Set Values
 *
 */

/* ARGSUSED */
static Boolean 
SetValuesHook(Widget w, ArgList args, Cardinal *num_args)
{
  Boolean ret = TRUE;
  int i;

  for (i = 0; i < *num_args; i++) {
    if (strcmp(XtNfile, args[i].name) == 0) {
      LoadFile(w);
      ret = TRUE;
    }
  }

/*
 * Changing anthing else will have strange effects, I don't need it so
 * I didn't code it.
 */

  return(ret);

} /* Set Values */

/* 
 * A little design philosophy is probabally wise to include at this point.
 *
 * One of the things that I has hoped to achieve with xman is to make the
 * viewing of manpage not only easy for the nieve user, but also fast for
 * the experienced user, I wanted to be able to use it too.  To achieve this
 * I end up sacrificing a bit of start up time for the manual data structure.
 * As well as, the overhead of reading the entire file before putting it up 
 * on the display.  This is actually hardly even noticeable since most manual
 * pages are shots, one to two pages - the notable exception is of course csh,
 * but then that should be broken up anyway. 
 *
 * METHOD:
 *
 * I allocate a chunk of space that is the size of the file, plus a null for
 * debugging.  Then copiesthe file into this chunk of memory.  I then allocate
 * an array of char*'s that are assigned to the beginning of each line.  Yes,
 * this means that I have to read the file twice, and could probabally be more
 * clever about it, but once it is in memory the second read is damn fast.
 * There are a few obsucrities here about guessing the number of lines and
 * reallocing if I guess wrong, but other than that it is pretty straight 
 * forward.
 *
 *                                         Chris Peterson
 *                                         1/27/88
 */

#define ADD_MORE_MEM 100	/* first guesses for allocations. */
#define CHAR_PER_LINE 40

/*	Function Name: LoadFile
 *	Description: Loads the current file into the internal memory.
 *	Arguments: w - the sblw.
 *	Returns: none
 */

static void
LoadFile(Widget w)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;
  FILE * file = sblw->scroll.file;

  char *page;
  char **line_pointer,**top_line; /* pointers to beginnings of the 
				     lines of the file. */
  int nlines;			/* the current number of allocated lines. */
  struct stat fileinfo;		/* file information from fstat. */

  if ( sblw->scroll.top_line != NULL) {
    XtFree(*(sblw->scroll.top_line)); /* free characters. */
    XtFree((char *)(sblw->scroll.top_line)); /* free lines. */
  }
  sblw->scroll.top_line = NULL;

  if (file == NULL)
    return;

/*
 * Get file size and allocate a chunk of memory for the file to be 
 * copied into.
 */

  if (fstat(fileno(file), &fileinfo))
    XtAppError(XtWidgetToApplicationContext(w), 
	       "SBLW LoadFile: Failure in fstat.");

/* 
 * Allocate a space for a list of pointer to the beginning of each line.
 */

  if ( (nlines = fileinfo.st_size/CHAR_PER_LINE) == 0)
    return;

  page = XtMalloc(fileinfo.st_size + 1); /* leave space for the NULL */
  top_line = line_pointer = (char**) XtMalloc( nlines * sizeof(char *) );

/*
 * Copy the file into memory. 
 */

  if (fread(page, sizeof(char), fileinfo.st_size, file) == 0)
    XtAppError(XtWidgetToApplicationContext(w), 
	       "SBLW LoadFile: Failure in fread.");


/* put NULL at end of buffer. */

  *(page + fileinfo.st_size) = '\0';

/*
 * Go through the file setting a line pointer to the character after each 
 * new line.  If we run out of line pointer space then realloc that space
 * with space for more lines.
 */

  *line_pointer++ = page;	/* first line points to first char in buffer.*/
  while (*page != '\0') {

    if ( *page == '\n' ) {
      *line_pointer++ = page + 1;

      if (line_pointer >= top_line + nlines) {
	top_line = (char **) XtRealloc( (char *)top_line, (nlines +
					  ADD_MORE_MEM) * sizeof(char *) );
	line_pointer = top_line + nlines;
	nlines += ADD_MORE_MEM;
      }
    }
    page++;
  }
   
/*
 *  Realloc the line pointer space to take only the minimum amount of memory
 */

  sblw->scroll.lines = nlines = line_pointer - top_line - 1;
  top_line = (char **) XtRealloc((char *)top_line, nlines * sizeof(char *));

/*
 * Store the memory pointers
 */

  sblw->scroll.top_line = top_line;
  sblw->scroll.line_pointer = 0;
  SetThumbHeight(w);
  SetThumb(w);
}

#define NLINES  66		/* This is the number of lines to wait until
				   we boldify the line again, this allows 
				   me to bold the first line of each page.*/
#define BACKSPACE 010		/* I doubt you would want to change this. */

#define NORMAL	0
#define BOLD	1
#define ITALIC	2
#define SYMBOL	3
#define WHICH(italic, bold)	((bold) ? BOLD : ((italic) ? ITALIC : NORMAL))
				/* Choose BOLD over ITALICS.  If neither */
				/* is chosen, use NORMAL. */

static int DumpText(Widget w, int x_loc, int y_loc, char * buf, int len, int format);
static Boolean Boldify(char *);

/*	Function Name: PrintText
 *	Description: This function actually prints the text.
 *	Arguments: w - the ScrollByLine widget.
 *                 start_line - line to start printing,
 *                 num_lines - the number of lines to print.
 *                 location - the location to print the text.
 *	Returns: none.
 */

/* ARGSUSED */

static void
PrintText(Widget w, int start_line, int num_lines, int location)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;

  register char *bufp, *c;	/* Generic char pointers */
  int current_line;		/* the number of the currrent line */
  char buf[BUFSIZ];		/* Misc. characters */
  Boolean italicflag = FALSE;	/* Print text in italics?? */
  Boolean first = TRUE;	        /* First line of a manual page??? */
  int x_loc, y_loc;		/* x and y location of text. */

/*
 * For table hack
 * To get columns to line up reasonably in most cases, make the
 * assumption that they were lined up using lots of spaces, where
 * lots is greater than two. Use a space width of 70% of the
 * widest character in the font.
 */
  int h_col, h_fix;
  char * h_c;

/*
 * Nothing loaded, take no action.
 */

  if (sblw->scroll.top_line == NULL || num_lines == 0)
    return;

  current_line = start_line;

/* Set the first character to print at the first line. */

  c = *(sblw->scroll.top_line + start_line);

/*
 * Because XDrawString uses the bottom of the text as a position
 * reference, add the height from the top of the font to the baseline
 * to the ScollByLine position reference.
 */

  y_loc = location + sblw->scroll.normal_font->max_bounds.ascent;

/*
 * Ok, here's the more than mildly heuristic man page formatter.
 * We put chars into buf until either a font change or newline
 * occurs (at which time we flush it to the screen.)
 */


  bufp = buf;
  x_loc = sblw->scroll.offset + sblw->scroll.indent;
  h_col = 0;
  
/*
 * A fix:
 * Assume that we are always starting to print on or before the
 * first line of a page, and then prove it if we aren't.
 */
  for (h_fix = 1; h_fix <= (start_line % NLINES); h_fix++)
    if (**(sblw->scroll.top_line + start_line - h_fix) != '\n')
      {
	first = FALSE;
	break;
      }

  while(TRUE) {
    if (current_line % NLINES == 0)
      first = TRUE;

/* 
 * Lets make sure that we do not run out of space in our buffer, making full
 * use of it is not critical since no window will be wide enough to display
 * nearly BUFSIZ characters.
 */

    if ( (bufp - buf) > (BUFSIZ - 10) )
      /* Toss everything until we find a <CR> or the end of the buffer. */
      while ( (*c != '\n') && (*c != '\0') ) c++;

    switch(*c) {

    case '\0':		      /* If we reach the end of the file then return */
      DumpText(w, x_loc, y_loc, buf, bufp - buf, WHICH(italicflag, first));
      return;

    case '\n':
      if (bufp != buf) {
	Boolean bold;
	*bufp = '\0';		/* for Boldify. */
	bold = ( (first) || ((x_loc == (sblw->scroll.offset +
			      sblw->scroll.indent)) && Boldify(buf)) );

	(void) DumpText(w, x_loc, y_loc, buf, bufp - buf,
			WHICH(italicflag, bold));

	if (bold)
	  first = FALSE;
      }

/* 
 * If we have painted the required number of lines then we should now return.
 */
      if (++current_line == start_line + num_lines ) 
	return;

      bufp = buf;
      italicflag = FALSE;
      x_loc = sblw->scroll.offset + sblw->scroll.indent;
      h_col = 0;
      y_loc += sblw->scroll.font_height;
      break;

/*
 * This tab handling code is not very clever it moves the cursor over
 * to the next boundry of eight (8) spaces, as calculated in width just
 * before the printing loop started.
 */

    case '\t':			/* TAB */
      x_loc = DumpText(w, x_loc, y_loc, buf, bufp - buf,
		       WHICH(italicflag, first));
      h_col += bufp - buf;
      bufp = buf; 
      italicflag = FALSE;
      x_loc = sblw->scroll.offset + sblw->scroll.indent;
      h_col = h_col + 8 - (h_col%8);
      x_loc += h_width * h_col;
      break;

    case ' ':
      h_c = c + 1;
      while (*h_c == ' ') h_c++;

      if (h_c - c < 4)
	{
	  *bufp++ = *c;
	  break;
	}

      x_loc = DumpText(w, x_loc, y_loc, buf, bufp - buf,
		       WHICH(italicflag, first));
      h_col += bufp - buf;
      bufp = buf; 
      italicflag = FALSE;

      x_loc = sblw->scroll.offset + sblw->scroll.indent; 
      h_col += (h_c - c);
      x_loc += h_width * h_col; 
      c = h_c - 1;
      break;

    case '\033':		/* ignore esc sequences for now */
      c++;			/* should always be esc-x */
      break;

/* 
 * Overstrike code supplied by: cs.utexas.edu!ut-emx!clyde@rutgers.edu 
 * Since my manual pages do not have overstrike I couldn't test this.
 */

    case BACKSPACE:		/* Backspacing for nroff bolding */
      if (c[-1] == c[1] && c[1] != BACKSPACE) {	/* overstriking one char */
        if (bufp > buf) {
	  bufp--;		/* Zap 1st instance of char to bolden */
	  x_loc = DumpText(w, x_loc, y_loc, buf, bufp - buf,
			   WHICH(italicflag, FALSE));
	  h_col += bufp - buf;
	}
	bufp = buf;
	*bufp++ = c[1];
	x_loc = DumpText(w, x_loc, y_loc, buf, bufp - buf, BOLD);
	h_col += bufp - buf;
	bufp = buf;
	first = FALSE;

	/*
	 *     Nroff bolding looks like:
	 * 	         C\bC\bC\bCN...
	 * c points to ----^      ^
	 * it needs to point to --^
	 */
	while (*c == BACKSPACE && c[-1] == c[1])
	  c += 2;
	c--;		/* Back up to previous char */
      }
      else {
	if ((c[-1] == 'o' && c[1] == '+')          /* Nroff bullet */
	    || (c[-1] == '+' && c[1] == 'o')) {	   /* Nroff bullet */
				/* If we run into a bullet, print out */
				/* everything that's accumulated to this */
				/* point, then the bullet, then resume. */
	  if (bufp>buf) {
	    bufp--;
	    x_loc = DumpText(w, x_loc, y_loc, buf, bufp - buf,
		 	     WHICH(italicflag, FALSE));
	    h_col += bufp - buf;
	  }
	  bufp = buf;
	  *bufp = (char)183;
	  x_loc = DumpText(w, x_loc, y_loc, buf, 1, SYMBOL);
	  h_col++;
	  c++;
	}
	else {		/* 'real' backspace - back up output ptr */
	  if (bufp>buf)
	    bufp--;
	}
      }
      break;

/* End of contributed overstrike code. */
  
   case '_':			/* look for underlining [italicize] */
      if(*(c + 1) == BACKSPACE) {
	if(!italicflag) {	/* font change? */
	  if (bufp != buf) {
	    x_loc = DumpText(w, x_loc, y_loc, buf, bufp - buf, NORMAL);
	    h_col += bufp - buf;
	    bufp = buf;
	  }
	  italicflag = TRUE;
	}
	c += 2;
	*bufp++ = *c;
	break;
      }
      /* else fall through to default, because this was a real underscore. */

    default:
      if(italicflag) { 			/* font change? */
	if (bufp != buf) {
	  x_loc = DumpText(w, x_loc, y_loc, buf, bufp - buf,
			   WHICH(italicflag, FALSE));
	  h_col += bufp - buf;
	  bufp = buf;
	}
	italicflag = FALSE;
      }
      *bufp++ = *c;
      break;
    }
    c++;
  }
}

/*	Function Name: DumpText
 *	Description: Dumps text to the screen.
 *	Arguments: w - the widget.
 *                 x_loc - to dump text at.
 *                 y_loc - the y_location to draw_text.
 *                 buf - buffer to dump.
 *                 italic, bold, boolean that tells us which gc to use.
 *	Returns: x_location of the end of the text.
 */

static int
DumpText(Widget w, int x_loc, int y_loc, char * buf, int len, int format)
{
  ScrollByLineWidget sblw = (ScrollByLineWidget) w;
  GC gc;
  XFontStruct * font;

  switch(format) {

  case ITALIC:
    gc = sblw->scroll.italic_gc;
    font = sblw->scroll.italic_font;
    break;

  case BOLD:
      gc = sblw->scroll.bold_gc;
      font = sblw->scroll.bold_font;
    break;

  case SYMBOL:
    gc = sblw->scroll.symbol_gc;
    font = sblw->scroll.symbol_font;
    break;

  default:
      gc = sblw->scroll.normal_gc;
      font = sblw->scroll.normal_font;
    break;
    }

  XDrawString(XtDisplay(w), XtWindow(w), gc, x_loc, y_loc, buf, len);
  return(x_loc + XTextWidth(font, buf, len));
}

/*	Function Name: Boldify
 *	Description: look for keyword.
 *	Arguments: sp - string pointer.
 *	Returns: 1 if keyword else 0.
 */

static Boolean
Boldify(register char *sp)
{
  register char *sp_pointer;
  int length,count;

/* 
 * If there are not lower case letters in the line the assume it is a
 * keyword and boldify it in PrintManpage.
 */

  length = strlen(sp);
  for (sp_pointer = sp, count = 0; count < length; sp_pointer++,count++) 
    if ( !isupper(*sp_pointer) && !isspace(*sp_pointer) )
      return(0);
  return(1);
}

#undef superclass
#undef SuperClass
