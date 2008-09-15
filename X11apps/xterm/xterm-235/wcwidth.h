/* $XFree86: xc/programs/xterm/wcwidth.h,v 1.5 2005/05/03 00:38:25 dickey Exp $ */

#ifndef	included_wcwidth_h
#define	included_wcwidth_h 1

#include <stddef.h>

extern int mk_wcswidth(const wchar_t * pwcs, size_t n);
extern int mk_wcswidth_cjk(const wchar_t * pwcs, size_t n);
extern int mk_wcwidth(wchar_t ucs);
extern int mk_wcwidth_cjk(wchar_t ucs);
extern int wcswidth_cjk(const wchar_t * pwcs, size_t n);

#endif /* included_wcwidth_h */
