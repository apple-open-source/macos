/* $XFree86: xc/lib/Xi/XIint.h,v 3.2 2003/07/07 15:34:22 eich Exp $ */

/*
 *	XIint.h - Header definition and support file for the internal
 *	support routines used by the Xi library.
 */

#ifndef _XIINT_H_
#define _XIINT_H_

extern XExtDisplayInfo *XInput_find_display(Display *);

extern int _XiCheckExtInit(Display *, int, XExtDisplayInfo *);

extern XExtensionVersion *_XiGetExtensionVersion(Display *, _Xconst char *, XExtDisplayInfo *);

#endif
