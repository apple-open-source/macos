/* $XTermId: fontutils.h,v 1.71 2009/02/13 01:36:41 tom Exp $ */

/************************************************************

Copyright 1998-2008,2009 by Thomas E. Dickey

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
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
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

********************************************************/

#ifndef included_fontutils_h
#define included_fontutils_h 1

#include <xterm.h>

extern Bool xtermLoadDefaultFonts (XtermWidget /* xw */);
extern Bool xtermOpenFont (XtermWidget /* xw */, const char */* name */, XTermFonts * /* result */, fontWarningTypes /* warn */, Bool /* force */);
extern XTermFonts * xtermCloseFont (XtermWidget /* xw */, XTermFonts * /* fnt */);
extern const VTFontNames * xtermFontName (char */* normal */);
extern int lookupRelativeFontSize (XtermWidget /* xw */, int /* old */, int /* relative */);
extern int xtermGetFont(const char * /* param */);
extern int xtermLoadFont (XtermWidget /* xw */, const VTFontNames */* fonts */, Bool /* doresize */, int /* fontnum */);
extern void HandleSetFont PROTO_XT_ACTIONS_ARGS;
extern void SetVTFont (XtermWidget /* xw */, int /* i */, Bool /* doresize */, const VTFontNames */* fonts */);
extern void xtermCloseFonts (XtermWidget /* xw */, XTermFonts * /* fnts[fMAX] */);
extern void xtermComputeFontInfo (XtermWidget /* xw */, VTwin */* win */, XFontStruct */* font */, int /* sbwidth */);
extern void xtermCopyFontInfo (XTermFonts * /* target */, XTermFonts * /* source */);
extern void xtermFreeFontInfo (XTermFonts * /* target */);
extern void xtermSaveFontInfo (TScreen * /* screen */, XFontStruct */* font */);
extern void xtermSetCursorBox (TScreen * /* screen */);
extern void xtermUpdateFontInfo (XtermWidget /* xw */, Bool /* doresize */);

#if OPT_DEC_CHRSET
extern char *xtermSpecialFont (TScreen */* screen */, unsigned /* atts */, unsigned /* chrset */);
#endif

#if OPT_BOX_CHARS
extern Bool xtermMissingChar (XtermWidget /* xw */, unsigned /* ch */, XFontStruct */* font */);
extern void xtermDrawBoxChar (XtermWidget /* xw */, unsigned /* ch */, unsigned /* flags */, GC /* gc */, int /* x */, int /* y */, int /* cols */);
#endif

#if OPT_LOAD_VTFONTS
extern void HandleLoadVTFonts PROTO_XT_ACTIONS_ARGS;
#endif

#if OPT_LOAD_VTFONTS || OPT_WIDE_CHARS
extern Bool xtermLoadWideFonts (XtermWidget /* w */, Bool /* nullOk */);
#endif

#define xtermIsDecGraphic(ch)	((ch) > 0 && (ch) < 32)

#if OPT_RENDERFONT
extern Bool xtermXftMissing (XtermWidget /* xw */, XftFont * /* font */, unsigned /* wc */);
#endif

#if OPT_SHIFT_FONTS
extern void HandleSmallerFont PROTO_XT_ACTIONS_ARGS;
extern void HandleLargerFont PROTO_XT_ACTIONS_ARGS;
#endif

#if OPT_WIDE_CHARS
extern unsigned ucs2dec (unsigned);
extern unsigned dec2ucs (unsigned);
#endif

#endif /* included_fontutils_h */
