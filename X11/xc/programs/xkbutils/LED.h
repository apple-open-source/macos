/* $Xorg: LED.h,v 1.3 2000/08/17 19:54:51 cpqbld Exp $ */
/************************************************************
 Copyright (c) 1995 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be 
 used in advertising or publicity pertaining to distribution 
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability 
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.
 
 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS 
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY 
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, 
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/
#ifndef _XawLED_h
#define _XawLED_h

#include <X11/Xaw/Simple.h>

/* Resources:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 background	     Background		Pixel		XtDefaultBackground
 bevel               Bevel              Dimension       1
 bitmap		     Pixmap		Pixmap		None
 border		     BorderColor	Pixel		XtDefaultForeground
 borderWidth	     BorderWidth	Dimension	1
 bottomColor         BottomColor        Pixel           "White"
 cursor		     Cursor		Cursor		None
 cursorName	     Cursor		String		NULL
 destroyCallback     Callback		XtCallbackList	NULL
 foreground	     Foreground		Pixel		XtDefaultForeground
 height		     Height		Dimension	10
 insensitiveBorder   Insensitive	Pixmap		Gray
 ledWidth            ledWidth           Dimension	6
 ledHeight           ledHeight          Dimension       12
 mappedWhenManaged   MappedWhenManaged	Boolean		True
 offColor	     OffColor		Pixel		#001000
 on		     On			Boolean		False
 onColor	     OnColor		Pixel		Green
 pointerColor	     Foreground		Pixel		XtDefaultForeground
 pointerColorBackground Background	Pixel		XtDefaultBackground
 resize		     Resize		Boolean		False
 sensitive	     Sensitive		Boolean		False
 topColor            TopColor           Pixel           "black"
 width		     Width		Dimension	6
 x		     Position		Position	0
 y		     Position		Position	0

*/

#define	XtNbevel	"bevel"
#define	XtNon		"on"
#define	XtNonColor	"onColor"
#define	XtNoffColor	"offColor"
#define	XtNtopColor	"topColor"
#define	XtNbottomColor	"bottomColor"
#define	XtNledWidth	"ledWidth"
#define	XtNledHeight	"ledHeight"
#define	XtCBevel	"Bevel"
#define	XtCOn		"On"
#define	XtCOnColor	"OnColor"
#define	XtCOffColor	"OffColor"
#define	XtCTopColor	"TopColor"
#define	XtCBottomColor	"BottomColor"
#define	XtCLedWidth	"LedWidth"
#define	XtCLedHeight	"LedHeight"

#ifndef _XtStringDefs_h_
#define XtNbitmap "bitmap"
#define XtNforeground "foreground"
#define XtNresize "resize"
#define XtCResize "Resize"
#define XtCBitmap "Bitmap"
#endif

/* Class record constants */

extern WidgetClass ledWidgetClass;

typedef struct _LEDClassRec *LEDWidgetClass;
typedef struct _LEDRec      *LEDWidget;

#endif /* _XawLED_h */
