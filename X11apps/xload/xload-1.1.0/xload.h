/* $XFree86: xc/programs/xload/xload.h,v 1.1 2001/08/27 23:35:14 dawes Exp $ */

#ifndef _XLOAD_H_
#define _XLOAD_H_

#include <X11/Intrinsic.h>

/*
 * Definition of the Application resources structure.
 */

typedef struct _XLoadResources {
  Boolean show_label;
  Boolean use_lights;
  String remote;
} XLoadResources;


extern void InitLoadPoint(void);
extern void GetLoadPoint(Widget w, XtPointer closure, XtPointer call_data);
extern void GetRLoadPoint(Widget w, XtPointer closure, XtPointer call_data);

extern XLoadResources resources;

#endif
