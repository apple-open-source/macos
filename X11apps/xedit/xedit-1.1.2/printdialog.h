/*
 * $Xorg: printdialog.h,v 1.1 2004/04/30 02:05:54 gisburn Exp $
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

#ifndef XMORE_PRINTDIALOG_H
#define XMORE_PRINTDIALOG_H 1

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Print.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/Form.h>
#include <X11/XprintUtil/xprintutil.h>

/* Resources:
   NOT WRITTEN YET
*/

#define XtCIcon "Icon"
#define XtNicon "icon"

#define XawNOkCallback             "okCallback"
#define XawNCancelCallback         "cancelCallback"

typedef struct
{
    Display            *pdpy;
    XPContext           pcontext;
    const char         *printToFileName;
    Boolean             printToFile;
    XpuColorspaceRec   *colorspace;
} XawPrintDialogCallbackStruct;

typedef struct _PrintDialogClassRec *PrintDialogWidgetClass;
typedef struct _PrintDialogRec      *PrintDialogWidget;

extern WidgetClass printDialogWidgetClass;

_XFUNCPROTOBEGIN

/* Prototypes */
void XawPrintDialogClosePrinterConnection(Widget w, Bool closeDisplay);

_XFUNCPROTOEND

#endif /* !XMORE_PRINTDIALOG_H */
