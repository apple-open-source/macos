/* $Xorg: Scale.h,v 1.4 2001/02/09 02:05:55 xorgcvs Exp $ */
/*

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/xmag/Scale.h,v 1.5 2001/12/14 20:02:11 dawes Exp $ */

#ifndef _XawScale_h
#define _XawScale_h

/***********************************************************************
 *
 * Scale Widget
 *
 ***********************************************************************/

#include <X11/Xaw/Simple.h>
#include <X11/Xmu/Converters.h>

/* Resources:

 All the SimpleWidget resources plus...
 Name                Class              RepType         Default Value
 ----                -----              -------         -------------
 aspectRatio         AspectRatio        Float           1.0
 autoscale           Autoscale          Boolean         True
 bufferSize          BufferSize         Cardinal        1024
 foreground          Foreground         Pixel           XtDefaultForeground
 gravity             Gravity            XtGravity       ForgetGravity
 image               Image              XImage*         NULL
 internalHeight      Height             Dimension       2
 internalWidth       Width              Dimension       2
 pasteBuffer         PasteBuffer        Boolean         False
 precision           Precision          Float           0.001
 proportional        Proportional       Boolean         False
 resize              Resize             Boolean         True
 scaleX              ScaleValue         Float           1.0
 scaleY              ScaleValue         Float           1.0
 userData            UserData           XtPointer       NULL
 visual              Visual             Visual*         CopyFromParent
*/

#ifndef _XtStringDefs_h_
#define XtNforeground "foreground"
#define XtNinternalWidth "internalWidth"
#define XtNinternalHeight "internalHeight"
#define XtNresize "resize"
#define XtCResize "Resize"
#endif

#define XtNaspectRatio "aspectRatio"
#define XtCAspectRatio "AspectRatio"
#define XtNbufferSize "bufferSize"
#define XtCBufferSize "BufferSize"
#define XtNscaleX "scaleX"
#define XtNscaleY "scaleY"
#define XtCScaleFactor "ScaleFactor"
#define XtNautoscale "autoscale"
#define XtCAutoscale "Autoscale"
#define XtNproportional "proportional"
#define XtCProportional "Proportional"
#define XtNprecision "precision"
#define XtCPrecision "Precision"
#define XtNgravity "gravity"
#define XtCGravity "Gravity"
#define XtNpasteBuffer "pasteBuffer"
#define XtCPasteBuffer "PasteBuffer"
#define XtNimage "image"
#define XtCImage "image"
#define XtNexponent "exponent"
#define XtCExponent "Exponent"
#define XtNuserData "userData"
#define XtCuserData "UserData"
#define XtRuserData "UserData"
#define XtRImage "Image"
#ifndef XtNvisual
#define XtNvisual "visual"
#endif
#define XtCvisual "Visual"
#define XtRvisual "Visual"


/* Class record constants */

extern WidgetClass scaleWidgetClass;

typedef struct _ScaleClassRec *ScaleWidgetClass;
typedef struct _ScaleRec      *ScaleWidget;

extern int SWGetImagePixel ( Widget w, Position x, Position y, 
			     Position *img_x, Position *img_y, 
			     Pixel *img_pixel );
extern void SWUnscale ( Widget w, XEvent *event, String *params, 
			Cardinal *num_params );
extern void SWAutoscale ( Widget w, XEvent *event, String *params, 
			  Cardinal *num_params );
extern void SWInitialSize ( Widget w, XEvent *event, String *params, 
			    Cardinal *num_params );
extern void SWSetImage ( Widget w, XImage *image );
extern void RequestSelection ( Widget w, XEvent *event, String *params, 
			       Cardinal *num_params );
extern void GrabSelection ( Widget w, XEvent *event, String *params, 
			    Cardinal *num_params );
extern Pixmap SWGetPixmap ( Widget w );

#endif /* _XawScale_h */

