/* $XFree86: xc/programs/xload/xload.h,v 1.2 2001/08/28 17:10:39 tsi Exp $ */

#ifndef _XLOAD_H_
#define _XLOAD_H_

#include <X11/Intrinsic.h>

extern void InitLoadPoint(void);
extern void GetLoadPoint(Widget w, XtPointer closure, XtPointer call_data);
extern void GetRLoadPoint(Widget w, XtPointer closure, XtPointer call_data);

#endif
