/* $XFree86: xc/lib/Xi/XIint.h,v 3.1 2001/08/01 00:44:39 tsi Exp $ */

/*
 *	XIint.h - Header definition and support file for the internal
 *	support routines used by the Xi library.
 */

#ifndef _XIINT_H_
#define _XIINT_H_

extern XExtDisplayInfo * XInput_find_display(
#if NeedFunctionPrototypes
	Display*
#endif
);

extern int _XiCheckExtInit(
#if NeedFunctionPrototypes
	Display*,
	int
#endif
);

#endif
