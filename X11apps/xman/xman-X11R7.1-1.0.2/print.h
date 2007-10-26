/*
 * $Xorg: print.h,v 1.1 2004/04/30 02:05:54 gisburn Exp $
 *
Copyright 2004 Roland Mainz <roland.mainz@nrubsig.org>

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 *
 */

#ifndef XMAN_PRINT_H
#define XMAN_PRINT_H 1

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Print.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/XprintUtil/xprintutil.h>

#define PrintMsg(x) { printf("xman: "); printf x ; }

/* Prototypes */
void DoPrintManpage(const char *programname,
                    FILE *manpage,
                    Widget toplevel, 
                    Display *pdpy, XPContext pcontext,
                    XpuColorspaceRec *colorspace,
                    XtCallbackProc printDisplayDestroyCallback,
                    const char *jobTitle,
                    const char *toFile);

#endif /* !XMAN_PRINT_H */
