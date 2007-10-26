/*
** $XConsortium: tests.c,v 1.20 91/06/08 18:57:07 rws Exp $
** $XFree86: xc/programs/xgc/tests.c,v 1.11tsi Exp $
**
*/

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xos.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "xgc.h"
#ifdef SVR4
#define SYSV
#endif
#if !defined(SYSV) && !defined(QNX4)
#include <sys/timeb.h>
#include <sys/resource.h>
#endif

#ifndef PI
#define PI 3.14159265
#endif

#ifdef SYSV
#define random lrand48
#endif


/* timer(flag)
** -----------
** When called with StartTimer, starts the stopwatch and returns nothing.
** When called with EndTimer, stops the stopwatch and returns the time
** in microseconds since it started.
**
** Uses rusage() so we can subtract the time used by the system and user
** from our timer, and just concentrate on the time used in the X calls.
*/

static long
timer(int flag)
{
#if !defined(SYSV)
  static struct timeval starttime;  /* starting time for gettimeofday() */
  struct timeval endtime;           /* ending time for gettimeofday() */
#if !defined(__UNIXOS2__) && !defined(QNX4)
  static struct rusage startusage;  /* starting time for getrusage() */
  struct rusage endusage;           /* ending time for getrusage() */
#endif
  struct timezone tz;               /* to make gettimeofday() happy */

  long elapsedtime;                 /* how long since we started the timer */

  switch (flag) {
    case StartTimer:                       /* store initial values */
      gettimeofday(&starttime,&tz);       
#if !defined(__UNIXOS2__) && !defined(QNX4)
      getrusage(RUSAGE_SELF,&startusage);
#endif
      return((long) NULL);
    case EndTimer:
      gettimeofday(&endtime,&tz);          /* store final values */
#if !defined(__UNIXOS2__) && !defined(QNX4)
      getrusage(RUSAGE_SELF,&endusage);
#endif

  /* all the following line does is use the formula 
     elapsed time = ending time - starting time, but there are three 
     different timers and two different units of time, ack... */

#if !defined(__UNIXOS2__) && !defined(QNX4)
      elapsedtime = (long) ((long)
	((endtime.tv_sec - endusage.ru_utime.tv_sec - endusage.ru_stime.tv_sec
	 - starttime.tv_sec + startusage.ru_utime.tv_sec
	 + startusage.ru_stime.tv_sec)) * 1000000) + (long)
      ((endtime.tv_usec - endusage.ru_utime.tv_usec - endusage.ru_stime.tv_usec
	 - starttime.tv_usec + startusage.ru_utime.tv_usec
	 + startusage.ru_stime.tv_usec));
#else
      elapsedtime = (long)( ((long)endtime.tv_sec-(long)starttime.tv_sec)*1000000
			   +((long)endtime.tv_usec-(long)starttime.tv_usec));
#endif
      return(elapsedtime);                

    default:                              
      fprintf(stderr,"Invalid flag in timer()\n");
      return((long) NULL);
    }
#else
  static time_t starttime;
  
  switch (flag) {
    case StartTimer:
      time(&starttime);
      return((long) NULL);
    case EndTimer:
      return( (time(NULL) - starttime) * 1000000);
    default:
      fprintf(stderr,"Invalid flag in timer()\n");
      return((long) NULL);
    }
#endif
}


void
copyarea_test(void)
{
  int num_copies = 200;
  int i;
  long totaltime;
  char buf[80];

  num_copies *= X.percent;

  XSetFillStyle(X.dpy,X.miscgc,FillTiled);
  XFillRectangle(X.dpy,X.win,X.miscgc,0,0,400,400);

  XSync(X.dpy,0);
  timer(StartTimer);
  for (i=0;i<num_copies;++i)
    XCopyArea(X.dpy,X.win,X.win,X.gc,i,200-i,
	      200,200,200-i,i);
  XSync(X.dpy,0);
  totaltime = timer(EndTimer);

  snprintf(buf,sizeof buf,"%.2f seconds.",(double)totaltime/1000000.);
  show_result(buf);
}

void
copyplane_test(void)
{
  int num_copies = 200;
  int i;
  long totaltime;
  char buf[80];

  if(!X.gcv.plane_mask || (X.gcv.plane_mask & (X.gcv.plane_mask - 1))) {
    show_result("exactly one bit in plane mask must be set for this test");
    return;
  }


  num_copies *= X.percent;

  XSetPlaneMask(X.dpy, X.gc, ~0L);
  XSetFillStyle(X.dpy,X.miscgc,FillTiled);
  XFillRectangle(X.dpy,X.win,X.miscgc,0,0,400,400);

  XSync(X.dpy,0);
  timer(StartTimer);
  for (i=0;i<num_copies;++i)
    XCopyPlane(X.dpy,X.win,X.win,X.gc,i,200-i,
	      200,200,200-i,i,X.gcv.plane_mask);
  XSync(X.dpy,0);
  totaltime = timer(EndTimer);
  XSetPlaneMask(X.dpy, X.gc, X.gcv.plane_mask);

  snprintf(buf,sizeof buf,"%.2f seconds.",(double)totaltime/1000000.);
  show_result(buf);
}

void
circle_line_test(int num_vertices, int radius)
{
  double theta, delta;
  int length, centerx, centery, i;
  int relative_angle;
  long totaltime;
  char buf[80];
  XPoint *coord;

  relative_angle = num_vertices*5/12+1;
  delta = (double) relative_angle / (double) num_vertices * 2 * PI;
  centerx = centery = 200;

  coord = (XPoint *) malloc (sizeof(XPoint)*(num_vertices+1));

  length = (int) (2 * radius * (float) atan(delta/2.));

  for (i=0;i<=num_vertices;++i) {
    theta = (double) i * delta;
    coord[i].x = centerx + (int) (radius * cos(theta));
    coord[i].y = centery + (int) (radius * sin(theta));
  }

  XSync(X.dpy,0);
  timer(StartTimer);
  XDrawLines(X.dpy,X.win,X.gc,coord,num_vertices+1,CoordModeOrigin);
  XSync(X.dpy,0);
  totaltime = timer(EndTimer);

  snprintf(buf,sizeof buf,"%d lines of length %d in %.3f seconds.",num_vertices,
	  length,(double)totaltime/1000000.);
  show_result(buf);

  free(coord);
}



void
polyline_test(void)
{
  circle_line_test((int)(601*X.percent),190);
}

void
polysegment_test(void)
{
  XSegment *segments;
  int num_segments = 600;
  long totaltime;
  char buf[80];
  int i;

  num_segments *= X.percent;

  segments = (XSegment *) malloc(sizeof(XSegment) * num_segments);

  segments[0].x1 = random()%400; segments[0].y1 = random()%400;
  segments[0].x2 = random()%400; segments[0].y2 = random()%400;

  for(i=1;i<num_segments;++i) {
    segments[i].x1 = (segments[i-1].x1-segments[i-1].y2+400+i)%400;
    segments[i].y1 = (segments[i-1].y1+segments[i-1].x2+i)%400;
    segments[i].x2 = (segments[i-1].x1-segments[i-1].y1+400+i)%400;
    segments[i].y2 = (segments[i-1].x2+segments[i-1].y2+i)%400;
  }

  XSync(X.dpy,0);
  start_timer();
  XDrawSegments(X.dpy,X.win,X.gc,segments,num_segments);
  XSync(X.dpy,0);
  totaltime = end_timer();
  
  snprintf(buf,sizeof buf,"%d segments in %.3f seconds.",num_segments,
	  (double)totaltime/1000000.);
  show_result(buf);

  free(segments);
}

void
polypoint_test(void)
{
  XPoint *points;
  int num_points = 100000;
  long totaltime;
  char buf[80];
  int i;
  
  num_points *= X.percent;

  points = (XPoint *) malloc(sizeof(XPoint) * num_points);

  points[0].x = random()%400; points[0].y = random()%400;
  points[1].x = random()%400; points[1].y = random()%400;

  for (i=2;i<num_points;++i) {
    points[i].x = (points[i-1].x+points[i-2].y+i*3/200)%400;
    points[i].y = (points[i-1].y+points[i-2].x+i*5/200)%400;
  }

  XSync(X.dpy,0);
  start_timer();
  XDrawPoints(X.dpy,X.win,X.gc,points,num_points,CoordModeOrigin);
  XSync(X.dpy,0);
  totaltime = end_timer();

  snprintf(buf,sizeof buf,"%d points in %.3f seconds.",num_points,
	  (double)totaltime/1000000.);
  show_result(buf);

  free(points);
}

void
genericrectangle_test(Boolean fill)
{
  XRectangle *rects;
  int num_rects = 200;
  int perimeter = 0, area = 0;
  int i;
  long totaltime;
  char buf[80];

  num_rects *= X.percent;

  rects = (XRectangle *) malloc(sizeof(XRectangle) * num_rects);

  for (i=0;i<num_rects;++i) {
    rects[i].x = rects[i].y = 200 - i;
    rects[i].width = rects[i].height = 2 * i;
    perimeter += rects[i].width * 2 + rects[i].height * 2;
    area += rects[i].width * rects[i].height;
  }

  XSync(X.dpy,0);
  start_timer();
  if (fill) XFillRectangles(X.dpy,X.win,X.gc,rects,num_rects);
  else XDrawRectangles(X.dpy,X.win,X.gc,rects,num_rects);
  XSync(X.dpy,0);
  totaltime = end_timer();

  if (fill)
    snprintf(buf,sizeof buf,"%d pixels in %.2f seconds.",area,(double)totaltime/1000000.);
  else
    snprintf(buf,sizeof buf,"Total line length %d in %.3f seconds.",perimeter,
	    (double)totaltime/1000000.);
  show_result(buf);

  free(rects);
}

void
polyrectangle_test(void)
{
  genericrectangle_test(FALSE);
}

void
polyfillrectangle_test(void)
{
  genericrectangle_test(TRUE);
}

/*****************************/

void
fillpolygon_test(void)
{
  int i;
  int points_per_side = 40;
  int spacing;
  XPoint *points;
  XPoint polypoints[3];

  points = (XPoint *) malloc (sizeof(XPoint) * points_per_side * 4);
  spacing = 400 / points_per_side;

  for (i = 0; i < points_per_side; ++i) {
    points[i].x = i * spacing;
    points[i].y = 0;

    points[i + points_per_side].x = 400;
    points[i + points_per_side].y = i * spacing;

    points[i + 2 * points_per_side].x = 400 - i * spacing;
    points[i + 2 * points_per_side].y = 400;

    points[i + 3 * points_per_side].x = 0;
    points[i + 3 * points_per_side].y = 400 - i * spacing;
  }

  for (i = 0; i < 2 * points_per_side; i += 2) {
    polypoints[0].x = points[i].x;
    polypoints[0].y = points[i].y;

    polypoints[1].x = points[i + 2 * points_per_side].x;
    polypoints[1].y = points[i + 2 * points_per_side].y;

    polypoints[2].x = points[i + 2 * points_per_side + 1].x;
    polypoints[2].y = points[i + 2 * points_per_side + 1].y;

    XFillPolygon (X.dpy, X.win, X.gc, polypoints, 3, Convex, CoordModeOrigin);
  }
  free(points);
}

/*****************************/

void
genericarc_test(Boolean fill)
{
  XArc *arcs;
  int num_arcs = 180;
  int i;
  long totaltime;
  char buf[80];

  num_arcs *= X.percent;

  arcs = (XArc *) malloc(sizeof(XArc) * num_arcs);

  for (i=0;i<num_arcs;++i) {
    arcs[i].x = i;
    arcs[i].y = i;
    arcs[i].width = i;
    arcs[i].height = i;
    arcs[i].angle1 = i * 128;
    arcs[i].angle2 = i * 128;
  }

  XSync(X.dpy,0);
  start_timer();
  if (fill) XFillArcs(X.dpy,X.win,X.gc,arcs,num_arcs);
  else XDrawArcs(X.dpy,X.win,X.gc,arcs,num_arcs);
  XSync(X.dpy,0);
  totaltime = end_timer();

  snprintf(buf,sizeof buf,"An uncounted number of pixels in %.3f seconds.",
	  (double)totaltime/1000000.);
  show_result(buf);

  free(arcs);
}

void
polyarc_test(void)
{
  genericarc_test(FALSE);
}

void
polyfillarc_test(void)
{
  genericarc_test(TRUE);
}

static const char string8[] = "pack my box with five dozen liquor jugs";

void
polytext8_test(void)
{
  int num_strings = 200;
  int i;
  long totaltime;
  char buf[80];

  num_strings *= X.percent;

  XSync(X.dpy,0);
  start_timer();
  for (i=0;i<num_strings;++i) {
    XDrawString(X.dpy,X.win,X.gc,(i%2 ? i : num_strings - i),i,
		string8,sizeof(string8)-1);
  }
  XSync(X.dpy,0);
  totaltime = end_timer();

  snprintf(buf,sizeof buf,"%d strings in %.2f seconds.",num_strings,
	  (double) totaltime/1000000.);
  show_result(buf);
}

void
imagetext8_test(void)
{
  int num_strings = 200;
  int i;
  long totaltime;
  char buf[80];

  num_strings *= X.percent;

  XSync(X.dpy,0);
  start_timer();
  for (i=0;i<num_strings;++i) {
    XDrawImageString(X.dpy,X.win,X.gc,(i%2 ? i : num_strings - i),i,
		     string8,sizeof(string8)-1);
  }
  XSync(X.dpy,0);
  totaltime = end_timer();

  snprintf(buf,sizeof buf,"%d strings in %.2f seconds.",num_strings,
	  (double) totaltime/1000000.);
  show_result(buf);
}

static char unicode_font[] =
  "-misc-fixed-medium-r-semicondensed--13-120-75-75-c-60-iso10646-1";

static const XChar2b string16[] = {
  { 0x00, 0x20 }, { 0x00, 0x20 }, { 0x22, 0x2E }, { 0x00, 0x20 },
  { 0x00, 0x45 }, { 0x22, 0xC5 }, { 0x00, 0x64 }, { 0x00, 0x61 },
  { 0x00, 0x20 }, { 0x00, 0x3D }, { 0x00, 0x20 }, { 0x00, 0x51 },
  { 0x00, 0x2C }, { 0x00, 0x20 }, { 0x00, 0x20 }, { 0x00, 0x6E },
  { 0x00, 0x20 }, { 0x21, 0x92 }, { 0x00, 0x20 }, { 0x22, 0x1E },
  { 0x00, 0x2C }, { 0x00, 0x20 }, { 0x22, 0x11 }, { 0x00, 0x20 },
  { 0x00, 0x66 }, { 0x00, 0x28 }, { 0x00, 0x69 }, { 0x00, 0x29 },
  { 0x00, 0x20 }, { 0x00, 0x3D }, { 0x00, 0x20 }, { 0x22, 0x0F },
  { 0x00, 0x20 }, { 0x00, 0x67 }, { 0x00, 0x28 }, { 0x00, 0x69 },
  { 0x00, 0x29 }, { 0x00, 0x2C }, { 0x00, 0x20 }, { 0x22, 0x00 },
  { 0x00, 0x78 }, { 0x22, 0x08 }, { 0x21, 0x1D }, { 0x00, 0x3A },
  { 0x00, 0x20 }, { 0x23, 0x08 }, { 0x00, 0x78 }, { 0x23, 0x09 },
  { 0x00, 0x20 }, { 0x00, 0x3D }, { 0x00, 0x20 }, { 0x22, 0x12 },
  { 0x23, 0x0A }, { 0x22, 0x12 }, { 0x00, 0x78 }, { 0x23, 0x0B },
  { 0x00, 0x2C }, { 0x00, 0x20 }, { 0x03, 0xB1 }, { 0x00, 0x20 },
  { 0x22, 0x27 }, { 0x00, 0x20 }, { 0x00, 0xAC }, { 0x03, 0xB2 },
  { 0x00, 0x20 }, { 0x00, 0x3D }, { 0x00, 0x20 }, { 0x00, 0xAC },
  { 0x00, 0x28 }, { 0x00, 0xAC }, { 0x03, 0xB1 }, { 0x00, 0x20 },
  { 0x22, 0x28 }, { 0x00, 0x20 }, { 0x03, 0xB2 }, { 0x00, 0x29 },
  { 0x00, 0x2C }
};

void
polytext16_test(void)
{
  int num_strings = 50;
  int i;
  long totaltime;
  char buf[80];

  num_strings *= X.percent;

  GC_change_font(unicode_font,FALSE);

  XSync(X.dpy,0);
  start_timer();
  for (i=0;i<num_strings;++i) {
    XDrawString16(X.dpy,X.win,X.gc,(i%2 ? i : num_strings - i),10*i,
		  string16,sizeof(string16)/sizeof(XChar2b));
  }
  XSync(X.dpy,0);
  totaltime = end_timer();

  GC_change_font(X.fontname,FALSE);

  snprintf(buf,sizeof buf,"%d strings in %.2f seconds.",num_strings,
	  (double) totaltime/1000000.);
  show_result(buf);
}

void
imagetext16_test(void)
{
  int num_strings = 50;
  int i;
  long totaltime;
  char buf[80];

  num_strings *= X.percent;

  GC_change_font(unicode_font,FALSE);

  XSync(X.dpy,0);
  start_timer();
  for (i=0;i<num_strings;++i) {
    XDrawImageString16(X.dpy,X.win,X.gc,(i%2 ? i : num_strings - i),10*i,
		       string16,sizeof(string16)/sizeof(XChar2b));
  }
  XSync(X.dpy,0);
  totaltime = end_timer();

  GC_change_font(X.fontname,FALSE);

  snprintf(buf,sizeof buf,"%d strings in %.2f seconds.",num_strings,
	  (double) totaltime/1000000.);
  show_result(buf);
}

void
putimage_test(void)
{
  int num_copies = 200;
  int i;
  long totaltime;
  char buf[80];

  num_copies *= X.percent;

  XSetFillStyle(X.dpy,X.miscgc,FillTiled);
  XFillRectangle(X.dpy,X.win,X.miscgc,0,0,400,400);

  X.image = XGetImage(X.dpy,X.win,0,0,200,200,~0,XYPixmap);

  XSync(X.dpy,0);
  timer(StartTimer);
  for (i=0;i<num_copies;++i)
    XPutImage(X.dpy,X.win,X.gc,X.image,0,0,i,i,200,200);
  XSync(X.dpy,0);
  totaltime = timer(EndTimer);

  snprintf(buf,sizeof buf,"%.2f seconds.",(double)totaltime/1000000.);
  show_result(buf);
}


/*****************************/
/*****************************/

void
run_test(void)
{
  XClearWindow(X.dpy,X.win);

  print_if_recording("run\n");
  
  switch (X.test) {
    case CopyArea:      copyarea_test();           break;
    case CopyPlane:     copyplane_test();          break;
    case PolyPoint:     polypoint_test();          break;
    case PolyLine:      polyline_test();           break;
    case PolySegment:   polysegment_test();        break;
    case PolyRectangle: polyrectangle_test();      break;
    case PolyArc:       polyarc_test();            break;
    case FillPolygon:   fillpolygon_test();        break;
    case PolyFillRect:  polyfillrectangle_test();  break;
    case PolyFillArc:   polyfillarc_test();        break;
    case PolyText8:     polytext8_test();          break;
    case ImageText8:    imagetext8_test();         break;
    case PolyText16:    polytext16_test();         break;
    case ImageText16:   imagetext16_test();        break;
    case PutImage:      putimage_test();           break;
    default: fprintf(stderr,"That test doesn't exist yet.\n");
    }
}

/*****************************/

/* set_text(w,string)
** ------------------
** Sets the text in a read-only text widget to the specified string.
*/

void
set_text(Widget w, char *string)
{
  static Arg args[2];

  XtSetArg(args[0], XtNstring, string);
  XtSetArg(args[1], XtNlength, strlen(string));
  XtSetValues(w, args, (Cardinal) 2 );
}

void
show_result(char *string)
{
  char buf[80];

  set_text(result,string);

  strcpy(buf,"# ");
  strncat(buf,string,sizeof(buf) - 3);
  buf[sizeof(buf) - 3] = '\0';
  strcat(buf,"\n");
  print_if_recording(buf);
}
