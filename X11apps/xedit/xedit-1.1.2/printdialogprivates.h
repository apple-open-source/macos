/*
 * $Xorg: printdialogprivates.h,v 1.1 2004/05/12 02:05:54 gisburn Exp $
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

#ifndef _XawPrintDialogP_h
#define _XawPrintDialogP_h 1

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xos.h>
#include <X11/Xmu/Misc.h>
#include <X11/CoreP.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/Command.h>    
#include <X11/Xaw/Label.h>
#include <X11/Xaw/DialogP.h>
#include <X11/Xaw/XawInit.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/FormP.h>

/* From Xaw/Privates.h */
#ifndef XtWidth
#define XtWidth(w)        (((RectObj)w)->rectangle.width)
#endif
#ifndef XtHeight
#define XtHeight(w)       (((RectObj)w)->rectangle.height)
#endif


typedef struct {
    XtPointer extension;
} PrintDialogClassPart;

typedef struct _PrintDialogClassRec {
    CoreClassPart         core_class;
    CompositeClassPart    composite_class;
    ConstraintClassPart   constraint_class;
    FormClassPart         form_class;
    PrintDialogClassPart  print_dialog_class;
} PrintDialogClassRec;

extern PrintDialogClassRec printDialogClassRec;

typedef struct _PrintDialogPart {
    /* resources */
    String	   label;		/* description of the dialog	 */
    String	   value;		/* for the user response	 */
    Pixmap	   icon;		/* icon bitmap			 */
    XtCallbackList ok_callback,
                   cancel_callback;
        
    /* private */
    Widget	iconW;		/* widget to display the icon	 */
    
    struct
    {
        Widget form,
               innerform,
               desclabel,
               desc,
               info,
               namelabel,
               name,
               selectprinter,
               filenamelabel,
               filename,
               selectfile,
               printtoprinter,
               printtofile,
               ok,
               setup,
               cancel;
    } main;

    Bool   printToFile;
    char  *filename;
    Bool   canPrint,       /* can we print ? */
           canReallyPrint; /* is it really save to print ? */
    XpuColorspaceRec *selected_colorspace;

    struct
    {
        Widget popup,
               form,
               list,
               ok,
               cancel;
    } selectPrinter;

    struct
    {
      Widget popup,
             form,
             paperlist,
             resolutionlist,
             orientationlist,
             plexlist,
             colorspacelist,
             jobcopieslabel,
             jobcopies,
             ok,
             cancel;
    } setup;

    struct
    {
        Widget dialog,
               shell;
    } selectFile;

    XPPrinterList  printerlist;
    int            num_printers;
    String        *widget_printerlist;

    String     printer_name;
    Display   *pdpy;
    XPContext  pcontext;

    XpuMediumSourceSizeList  paperlist;
    int                      num_papers;
    String                  *widget_paperlist;
    
    XpuResolutionList        resolutionlist;
    int                      num_resolutions;
    String                  *widget_resolutionlist;

    XpuOrientationList       orientationlist;
    int                      num_orientations;
    String                  *widget_orientationlist;

    XpuPlexList              plexlist;
    int                      num_plex;
    String                  *widget_plexlist;

    XpuColorspaceList        colorspacelist;
    int                      num_colorspaces;
    String                  *widget_colorspacelist;
} PrintDialogPart;

typedef struct _PrintDialogRec {
    CorePart            core;
    CompositePart       composite;
    ConstraintPart      constraint;
    FormPart            form;
    PrintDialogPart     printdialog;
} PrintDialogRec;

typedef struct {
    XtPointer extension;
} PrintDialogConstraintsPart;

typedef struct _PrintDialogConstraintsRec {
    FormConstraintsPart        form;
    PrintDialogConstraintsPart printdialog;
} PrintDialogConstraintsRec, *PrintDialogConstraints;

#endif /* !_XawPrintDialogP_h */

