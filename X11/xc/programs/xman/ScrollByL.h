/* $XConsortium: ScrollByL.h,v 1.7 94/04/17 20:43:48 dave Exp $ */
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

#ifndef _XtScrollByLine_h
#define _XtScrollByLine_h

/***********************************************************************
 *
 * ScrollByLine Widget (subclass of Simple)
 *
 ***********************************************************************/

/*
 * The default fonts.
 */

#ifdef ATHENA
#define MANPAGE_NORMAL   "fixed"
#define MANPAGE_BOLD     "helvetica-bold12"
#define MANPAGE_ITALIC   "helvetica-boldoblique12"
#define MANPAGE_SYMBOL   "symbol-medium12"
#else
#define MANPAGE_NORMAL   "*-new century schoolbook-medium-r-normal--*-120-*"
#define MANPAGE_BOLD     "*-new century schoolbook-bold-r-normal--*-120-*"
#define MANPAGE_ITALIC   "*-new century schoolbook-bold-i-normal--*-120-*"
#define MANPAGE_SYMBOL   "*-symbol-medium-r-normal--*-120-*"
#endif /* ATHENA */

#define XtNindent           "indent"
#define XtNforceVert        "forceVert"
#define XtNmanualFontNormal "manualFontNormal"
#define XtNmanualFontBold   "manualFontBold"
#define XtNmanualFontItalic "manualFontItalic"
#define XtNmanualFontSymbol "manualFontSymbol"

#define XtCIndent           "Indent"

/* Class record constants */

extern WidgetClass scrollByLineWidgetClass;

typedef struct _ScrollByLineClassRec *ScrollByLineWidgetClass;
typedef struct _ScrollByLineRec      *ScrollByLineWidget;

#endif /* _XtScrollByLine_h --- DON'T ADD STUFF AFTER THIS LINE */
