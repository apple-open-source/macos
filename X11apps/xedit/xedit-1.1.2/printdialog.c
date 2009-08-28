/*
 * $Xorg: printdialog.c,v 1.1 2004/05/12 02:05:54 gisburn Exp $
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

/* Force ANSI C prototypes from X11 headers */
#ifndef FUNCPROTO 
#define FUNCPROTO 15
#endif /* !FUNCPROTO */

#include <X11/StringDefs.h> 
#include <X11/Intrinsic.h> 
#include <X11/Shell.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Dialog.h>

#ifdef XKB
#include <X11/extensions/XKBbells.h>
#endif /* XKB */

#ifdef XEDIT
#include "xedit.h"
#endif /* XEDIT */
#include "printdialog.h"
#include "printdialogprivates.h"
#include "print.h"

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

/* Turn a NULL pointer string into an empty string */
#define NULLSTR(x) (((x)!=NULL)?(x):(""))

#define Error(x) { printf x ; exit(EXIT_FAILURE); }
#define Assertion(expr, msg) { if (!(expr)) { Error msg } }
#ifdef DEBUG
#  define Log(x)   { if(True)  printf x; }
#else
#  define Log(x)   { if(False) printf x; }
#endif /* DEBUG */

/* Local prototypes */
static void     do_beep(PrintDialogWidget pdw);
static void     center_popup(Widget parent, Widget popup);
static void     updateWidgetStates(PrintDialogWidget pdw);
static void     printSetupClose(PrintDialogWidget pdw);
static void     printSetupOKXtProc(Widget w, XtPointer client_data, XtPointer callData);
static void     printSetupCancelXtProc(Widget w, XtPointer client_data, XtPointer callData);
static void     printSetupJobCopiesXtProc(Widget w, XtPointer client_data, XtPointer callData);
static void     printDialogDestXtProc(Widget w, XtPointer client_data, XtPointer callData);
static void     printSelectFileXtProc(Widget w, XtPointer client_data, XtPointer callData);
static void     printOKXtProc(Widget w, XtPointer client_data, XtPointer callData);
static void     printSetupXtProc(Widget w, XtPointer client_data, XtPointer callData);
static void     printFileSelectedXtProc(Widget w, XtPointer client_data, XtPointer callData);
static void     buildFileDialog(PrintDialogWidget pdw);
static void     closePrinterConnection(PrintDialogWidget  pdw, Bool closeDisplay);
static Bool     openPrinterConnection(PrintDialogWidget  pdw);
static void     destroyPrintSetupDialog(PrintDialogWidget pdw);
static Widget   buildPrintSetupDialog(PrintDialogWidget pdw);
static String  *xpprinterlist_to_widget_printerlist(XPPrinterList printerlist, int num_printers);
static String  *xppaperlist_to_widget_paperlist(XpuMediumSourceSizeList paperlist, int num_papers);
static String  *xpresolutionlist_to_widget_resolutionlist(XpuResolutionList reslist, int num_resolutions);
static String  *xporientationlist_to_widget_orientationlist(XpuOrientationList orientationlist, int num_orientations);
static String  *xpplexlist_to_widget_plexlist(XpuPlexList plexlist, int num_plex);
static String  *xpcolorspacelist_to_widget_colorspacelist(XpuColorspaceList colorspacelist, int num_colorspaces);
static void     printerSelectionPrinterSelectedXtProc(Widget w, XtPointer client_data, XtPointer callData);
static void     printSelectPrinterXtProc(Widget w, XtPointer client_data, XtPointer callData);
static void     printerSelectionClose(PrintDialogWidget  pdw);
static void     printerSelectionOKXtProc(Widget w, XtPointer client_data, XtPointer callData);
static void     printerSelectionCancelXtProc(Widget w, XtPointer client_data, XtPointer callData);
static Bool     buildPrinterSelectionDialog(PrintDialogWidget  pdw);
static void     destroyPrinterSelectionDialog(PrintDialogWidget pdw);
static void     createprintdialogchildren(Widget w);

static
void do_beep(PrintDialogWidget pdw)
{
    Log(("*** Beep!\n"));
#ifdef XKB
    XkbStdBell(XtDisplay((Widget)pdw), XtWindow((Widget)pdw), 0, XkbBI_MinorError);
#else
    XBell(XtDisplay((Widget)pdw), 0);
#endif /* XKB */
}

/* Center popup (on parent, not on screen - which would be a bad idea
 * in the Xinerama case) */
static
void center_popup(Widget parent, Widget popup)
{
    Dimension width,
              height;
    Position  x,
              y;

    XtVaGetValues(parent,
        XtNx,  &x,
        XtNy,  &y,
        NULL);
    XtVaGetValues(popup,
        XtNwidth,  &width,
        XtNheight, &height,
        NULL);

    x += (Position)width  / 2;
    y += (Position)height / 3;

    XtVaSetValues(popup,
        XtNx, x,
        XtNy, y,
        NULL);
}

static
void updateWidgetStates(PrintDialogWidget pdw)
{
    PrintDialogPart *pdp = &pdw->printdialog;
    
    /* Do not update anything if we will be destroyed */
    if (pdw->core.being_destroyed)
        return;

    if (pdp->printToFile == False) {  
        XtVaSetValues(pdp->main.printtoprinter, XtNstate,     True,  NULL);
        XtVaSetValues(pdp->main.printtofile,    XtNstate,     False, NULL);
        /* Disable the widgets which depend on print-to-file */
        XtVaSetValues(pdp->main.filenamelabel,  XtNsensitive, False, NULL);
        XtVaSetValues(pdp->main.filename,       XtNsensitive, False, NULL);
        XtVaSetValues(pdp->main.selectfile,     XtNsensitive, False, NULL);
    }
    else
    {
        XtVaSetValues(pdp->main.printtoprinter, XtNstate, False, NULL);
        XtVaSetValues(pdp->main.printtofile,    XtNstate, True,  NULL);
        /* Enable the widgets which depend on print-to-file */
        XtVaSetValues(pdp->main.filenamelabel,  XtNsensitive, True, NULL);
        XtVaSetValues(pdp->main.filename,       XtNsensitive, True, NULL);
        XtVaSetValues(pdp->main.selectfile,     XtNsensitive, True, NULL);
    }

    /* Update file name */
    XtVaSetValues(pdp->main.filename, XtNlabel, pdp->filename?pdp->filename:"-", NULL);
    
    /* General check whether we can print... */
    pdp->canPrint = True;
    if (pdp->printer_name == NULL) {
        pdp->canPrint = False;
    }
    else if (strlen(pdp->printer_name) == 0) {
        pdp->canPrint = False;
    }

    pdp->canReallyPrint = pdp->canPrint;

    /* Some extra checks for print-to-file */
    if (pdp->printToFile) {
        if (pdp->filename == NULL) {
            pdp->canReallyPrint = False;
        }
        else if (strlen(pdp->filename) == 0) {
            pdp->canReallyPrint = False;
        }
    }

    XtVaSetValues(pdp->main.ok,    XtNsensitive, pdp->canReallyPrint, NULL);
    XtVaSetValues(pdp->main.setup, XtNsensitive, pdp->canPrint,       NULL);
}

static void
printSetupClose(PrintDialogWidget pdw)
{ 
    PrintDialogPart *pdp = &pdw->printdialog;
    if (pdp->setup.popup) {
        XtPopdown(pdp->setup.popup);
    }
}

static void
printSetupOKXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget    pdw = (PrintDialogWidget)client_data;
    PrintDialogPart     *pdp = &pdw->printdialog;
    char                *s;
    XawListReturnStruct *lrs;    
    Log(("## printSetupOK: closing print setup!\n"));
    
    /* Set copy count (if the value is invalid beep and return to the user ) */
    XtVaGetValues(pdp->setup.jobcopies, XtNstring, &s, NULL);
    if (s) {
        int c = atoi(s);
        if (c < 1 || c > 999) {
            do_beep(pdw);
            XtVaSetValues(pdp->setup.jobcopies, XtNstring, "1", NULL);
            Log(("## printSetupOKXtProc: not a valid copy count value\n"));
            return;
        }
        
        /* Fix me: We should set the number of _job_ copies here, not _doc_ copies */
        XpuSetDocumentCopies(pdp->pdpy, pdp->pcontext, c);
    }
    
    /* Set paper size */
    lrs = XawListShowCurrent(pdp->setup.paperlist);
    if (lrs->list_index != XAW_LIST_NONE) {
        Log(("selected paper is '%s'/'%d'\n", lrs->string, lrs->list_index));
        Assertion(lrs->list_index < pdp->num_papers, (("Error: lrs->list_index < pdp->num_papers\n")));
        XpuSetDocMediumSourceSize(pdp->pdpy, pdp->pcontext, &pdp->paperlist[lrs->list_index]);
    }
    XtFree((char *)lrs);

    /* Set resolution */
    lrs = XawListShowCurrent(pdp->setup.resolutionlist);
    if (lrs->list_index != XAW_LIST_NONE) {
        Log(("selected resolution is '%s'/'%d'\n", lrs->string, lrs->list_index));
        Assertion(lrs->list_index < pdp->num_resolutions, (("Error: lrs->list_index < pdp->num_resolutions\n")));
        XpuSetDocResolution(pdp->pdpy, pdp->pcontext, &pdp->resolutionlist[lrs->list_index]);
    }
    XtFree((char *)lrs);

    /* Set orientation */
    lrs = XawListShowCurrent(pdp->setup.orientationlist);
    if (lrs->list_index != XAW_LIST_NONE) {
        Log(("selected orientation is '%s'/'%d'\n", lrs->string, lrs->list_index));
        Assertion(lrs->list_index < pdp->num_orientations, (("Error: lrs->list_index < pdp->num_orientations\n")));
        XpuSetDocOrientation(pdp->pdpy, pdp->pcontext, &pdp->orientationlist[lrs->list_index]);
    }
    XtFree((char *)lrs);

    /* Set plex mode */
    lrs = XawListShowCurrent(pdp->setup.plexlist);
    if (lrs->list_index != XAW_LIST_NONE) {
        Log(("selected plex is '%s'/'%d'\n", lrs->string, lrs->list_index));
        Assertion(lrs->list_index < pdp->num_plex, (("Error: lrs->list_index < pdp->num_plex\n")));
        XpuSetDocPlex(pdp->pdpy, pdp->pcontext, &pdp->plexlist[lrs->list_index]);
    }
    XtFree((char *)lrs);

    /* Set colorspace */
    lrs = XawListShowCurrent(pdp->setup.colorspacelist);
    if (lrs->list_index != XAW_LIST_NONE) {
        Log(("selected colorspace is '%s'/'%d'\n", lrs->string, lrs->list_index));
        Assertion(lrs->list_index < pdp->num_colorspaces, (("Error: lrs->list_index < pdp->num_colorspaces\n")));
        pdp->selected_colorspace = &pdp->colorspacelist[lrs->list_index];
    }
    XtFree((char *)lrs);
    
    printSetupClose(pdw);
}

static void
printSetupCancelXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget  pdw = (PrintDialogWidget)client_data;
    Log(("## printSetupCancel: closing print setup!\n"));

    printSetupClose(pdw);
}

static void
printSetupJobCopiesXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget pdw    = (PrintDialogWidget)client_data;
    PrintDialogPart  *pdp    = &pdw->printdialog;
    char             *string = NULL;
    
    Log(("## printSetupJobCopiesXtProc!\n"));

    /* Check whether the input is a valid number - and in the case of invalid input reset the value */    
    XtVaGetValues(pdp->setup.jobcopies, XtNstring, &string, NULL);
    if (string) {
        char *s;
        int   i;
        Bool  isValidNumber = True;

        /* First check for invalid characters... */
        for( s = string ; *s != '\0' ; s++ ) {
            if (!isdigit(*s)) {
                isValidNumber = False;
                break;
            }
        }
        
        /* .. and the do a range check... */
        i = atoi(string);
        if (i < 1 || i > 999) {
            isValidNumber = False;
        }
        
        if (!isValidNumber) {
            do_beep(pdw);
            XtVaSetValues(pdp->setup.jobcopies, XtNstring, "1", NULL);
            Log(("## printSetupJobCopiesXtProc: not a valid number\n"));
        }
    }
}

static void
printCancelXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget pdw = (PrintDialogWidget)client_data;
    PrintDialogPart  *pdp = &pdw->printdialog;

    printSetupClose(pdw);

    if (pdp->cancel_callback) {
        Log(("printCancelXtProc: calling callback\n"));
        XtCallCallbackList((Widget)pdw, pdp->cancel_callback, NULL);
    }
}

static void
printDialogDestXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget  pdw = (PrintDialogWidget)client_data;
    PrintDialogPart   *pdp = &pdw->printdialog;

    if (w == pdp->main.printtoprinter) {  
        pdp->printToFile = False;
    }
    else if (w == pdp->main.printtofile) {
        pdp->printToFile = True;
    }
    else
    {
        Log(("printDialogDestXtProc: ERROR: Unknown widget.\n"));
    }

    updateWidgetStates(pdw);
}

static void
printSelectFileXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget   pdw = (PrintDialogWidget)client_data;
    PrintDialogPart    *pdp = &pdw->printdialog;

    Log(("## printSelectFileXtProc!\n"));

    if (!pdp->selectFile.shell) {
        buildFileDialog(pdw);
    }
    
    center_popup((Widget)pdw, pdp->selectFile.shell);

    XtPopup(pdp->selectFile.shell, XtGrabExclusive);
}

static void
printOKXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget  pdw = (PrintDialogWidget)client_data;
    PrintDialogPart   *pdp = &pdw->printdialog;
    Log(("## printing!\n"));
    
    if (!pdp->pdpy) {
        Log(("printOKXtProc: no printer connection, opening one....\n"));
        openPrinterConnection(pdw);

        if (!pdp->pdpy) {
            Log(("printOKXtProc: still no printer connection - BAD (this should never happen).\n"));
            do_beep(pdw);
            return;
        }
    }    

    if (pdp->ok_callback) {
        XawPrintDialogCallbackStruct pdcs;

        Log(("printOKXtProc: calling callbacks...\n"));
        memset(&pdcs, 0, sizeof(pdcs));
        pdcs.pdpy            = pdp->pdpy;
        pdcs.pcontext        = pdp->pcontext;
        pdcs.printToFile     = pdp->printToFile;
        pdcs.printToFileName = (const char *)pdp->filename;
        pdcs.colorspace      = pdp->selected_colorspace;
        XtCallCallbackList((Widget)pdw, pdp->ok_callback, &pdcs);
    }

    printSetupClose(pdw);
}

static void
printSetupXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget   pdw = (PrintDialogWidget)client_data;
    PrintDialogPart    *pdp = &pdw->printdialog;

    Log(("## setup!\n"));

    if (!pdp->setup.popup) {
        if (buildPrintSetupDialog(pdw) == NULL) {
            Log(("printSetupXtProc: buildPrintSetupDialog failure.\n"));
            return;
        }
    }

    center_popup((Widget)pdw, pdp->setup.popup);

    XtPopup(pdp->setup.popup, XtGrabExclusive);
}

static void
printFileSelectedXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget  pdw = (PrintDialogWidget)client_data;
    PrintDialogPart   *pdp = &pdw->printdialog;
    const char        *filename;
    
    Log(("printFileSelectedXtProc: User selected file.\n"));
    filename = XawDialogGetValueString(pdp->selectFile.dialog);
    
    if (pdp->filename)
        free(pdp->filename);
    pdp->filename = strdup(filename);
    
    XtPopdown(pdp->selectFile.shell);
    
    updateWidgetStates(pdw);
}

static
void buildFileDialog(PrintDialogWidget pdw)
{
    PrintDialogPart *pdp = &pdw->printdialog;
    int              n;
    Arg              args[20];

    pdp->selectFile.shell = XtCreatePopupShell("selectfile",
                                               transientShellWidgetClass,
                                               (Widget)pdw, NULL, 0);
    n = 0;
    XtSetArg(args[n], XtNvalue, pdp->filename?pdp->filename:""); n++;  
    pdp->selectFile.dialog = XtCreateManagedWidget("dialog", dialogWidgetClass,
                                                   pdp->selectFile.shell, args, n);
    XawDialogAddButton(pdp->selectFile.dialog, "Accept", printFileSelectedXtProc, pdw);

    XtRealizeWidget(pdp->selectFile.shell);
}

static
void closePrinterConnection(PrintDialogWidget pdw, Bool closeDisplay)
{ 
    PrintDialogPart *pdp = &pdw->printdialog;
    
    Log(("# closePrinterConnection\n"));

    destroyPrintSetupDialog(pdw);

    if (pdp->paperlist) {
        XpuFreeMediumSourceSizeList(pdp->paperlist);
        pdp->paperlist = NULL;
    }

    if (pdp->widget_paperlist) {
        free(pdp->widget_paperlist);
        pdp->widget_paperlist = NULL;
    }

    if (pdp->resolutionlist) {
        XpuFreeResolutionList(pdp->resolutionlist);
        pdp->resolutionlist = NULL;
    }

    if (pdp->widget_resolutionlist) {
        free(pdp->widget_resolutionlist);
        pdp->widget_resolutionlist = NULL;
    }


    if (pdp->orientationlist) {
        XpuFreeOrientationList(pdp->orientationlist);
        pdp->orientationlist = NULL;
    }  

    if (pdp->widget_orientationlist) {
        free(pdp->widget_orientationlist);
        pdp->widget_orientationlist = NULL;
    }

    if (pdp->plexlist) {
        XpuFreePlexList(pdp->plexlist);
        pdp->plexlist = NULL;
    }  

    if (pdp->widget_plexlist) {
        free(pdp->widget_plexlist);
        pdp->widget_plexlist = NULL;
    }

    if (pdp->colorspacelist) {
        XpuFreeColorspaceList(pdp->colorspacelist);
        pdp->colorspacelist = NULL;
    }  

    if (pdp->widget_colorspacelist) {
        free(pdp->widget_colorspacelist);
        pdp->widget_colorspacelist = NULL;
    }
    pdp->selected_colorspace = NULL;

    if (pdp->pdpy) {
        if (closeDisplay) {
            XpuClosePrinterDisplay(pdp->pdpy, pdp->pcontext);
        }
        pdp->pdpy     = NULL;
        pdp->pcontext = None;
    }

    updateWidgetStates(pdw);
}

static
Bool openPrinterConnection(PrintDialogWidget pdw)
{
    PrintDialogPart   *pdp = &pdw->printdialog;

    Log(("# openPrinterConnection\n"));

    /* Close any outstanding connection first... */
    closePrinterConnection(pdw, True);

    if (!pdp->printer_name) {
        Log(("# error: openPrinterConnection: No printer name.\n"));
        return False;
    }

    if (XpuGetPrinter(pdp->printer_name, &pdp->pdpy, &pdp->pcontext) == False) {
        Log(("openPrinterConnection: could not open printer.\n"));
        return False;
    }

    pdp->paperlist       = XpuGetMediumSourceSizeList(pdp->pdpy, pdp->pcontext, &pdp->num_papers);
    pdp->resolutionlist  =       XpuGetResolutionList(pdp->pdpy, pdp->pcontext, &pdp->num_resolutions);
    pdp->orientationlist =      XpuGetOrientationList(pdp->pdpy, pdp->pcontext, &pdp->num_orientations);
    pdp->plexlist        =             XpuGetPlexList(pdp->pdpy, pdp->pcontext, &pdp->num_plex);  
    pdp->colorspacelist  =       XpuGetColorspaceList(pdp->pdpy, pdp->pcontext, &pdp->num_colorspaces);  

    pdp->widget_paperlist       = xppaperlist_to_widget_paperlist(pdp->paperlist, pdp->num_papers);
    pdp->widget_resolutionlist  = xpresolutionlist_to_widget_resolutionlist(pdp->resolutionlist, pdp->num_resolutions);
    pdp->widget_orientationlist = xporientationlist_to_widget_orientationlist(pdp->orientationlist, pdp->num_orientations);
    pdp->widget_plexlist        = xpplexlist_to_widget_plexlist(pdp->plexlist, pdp->num_plex);
    pdp->widget_colorspacelist  = xpcolorspacelist_to_widget_colorspacelist(pdp->colorspacelist, pdp->num_colorspaces);

    updateWidgetStates(pdw);

    return True;
}

#undef DEFAULT_WIDTH
#define DEFAULT_WIDTH 120
#undef DEFAULT_INFOLABEL_WIDTH
#define DEFAULT_INFOLABEL_WIDTH 250

static
void destroyPrintSetupDialog(PrintDialogWidget pdw)
{
    PrintDialogPart *pdp = &pdw->printdialog;

    if (pdp->setup.popup) {
        XtDestroyWidget(pdp->setup.popup);
    }

    /* |XtDestroyWidget(pdp->setup.popup);| above will recursively destroy
     * all children so we only have to reset the pointers here... */
    pdp->setup.popup            = NULL;
    pdp->setup.form             = NULL;
    pdp->setup.paperlist        = NULL;
    pdp->setup.resolutionlist   = NULL;
    pdp->setup.orientationlist  = NULL;
    pdp->setup.plexlist         = NULL;
    pdp->setup.colorspacelist   = NULL;
    pdp->setup.ok               = NULL;
    pdp->setup.cancel           = NULL;
}

static
Widget buildPrintSetupDialog(PrintDialogWidget pdw)
{
    PrintDialogPart  *pdp = &pdw->printdialog;
    int               n;
    Arg               args[20];
    Widget            listform;
    XpuSupportedFlags jobflags,
                      docflags;
    Bool              canChangeJobCopies,
                      canChangePaperSize,
                      canChangeResolution,
                      canChangeOrientation,
                      canChangePlex;

    if (!pdp->pdpy) {
        Log(("buildPrintSetupDialog: no printer connection, opening one....\n"));
        openPrinterConnection(pdw);

        if (!pdp->pdpy) {
            Log(("buildPrintSetupDialog: still no printer connection - BAD.\n"));
            do_beep(pdw);
            return NULL;
        }
    }

    /* Get flags which types of attributes can be modified */
    jobflags = XpuGetSupportedDocAttributes(pdp->pdpy, pdp->pcontext);
    docflags = XpuGetSupportedDocAttributes(pdp->pdpy, pdp->pcontext);
    canChangeJobCopies   = (jobflags & XPUATTRIBUTESUPPORTED_COPY_COUNT) == XPUATTRIBUTESUPPORTED_COPY_COUNT;
    canChangePaperSize   = (docflags & XPUATTRIBUTESUPPORTED_DEFAULT_MEDIUM) == XPUATTRIBUTESUPPORTED_DEFAULT_MEDIUM;
    canChangeResolution  = (docflags & XPUATTRIBUTESUPPORTED_DEFAULT_PRINTER_RESOLUTION) == XPUATTRIBUTESUPPORTED_DEFAULT_PRINTER_RESOLUTION;
    canChangeOrientation = (docflags & XPUATTRIBUTESUPPORTED_CONTENT_ORIENTATION) == XPUATTRIBUTESUPPORTED_CONTENT_ORIENTATION;
    canChangePlex        = (docflags & XPUATTRIBUTESUPPORTED_PLEX) == XPUATTRIBUTESUPPORTED_PLEX;

    n = 0;
    XtSetArg(args[n], XtNallowShellResize, True); n++;
    pdp->setup.popup = XtCreatePopupShell("setup", transientShellWidgetClass, (Widget)pdw, args, n);

    n = 0;
    pdp->setup.form = XtCreateManagedWidget("form", formWidgetClass, pdp->setup.popup, args, n);

    n = 0;
    listform = XtCreateManagedWidget("list", formWidgetClass, pdp->setup.form, args, n);

    n = 0;
    XtSetArg(args[n], XtNresizable,           True);                        n++;
    XtSetArg(args[n], XtNforceColumns,        True);                        n++;
    XtSetArg(args[n], XtNdefaultColumns,      1);                           n++;
    XtSetArg(args[n], XtNsensitive,           canChangePaperSize);          n++;
    XtSetArg(args[n], XtNlist,                pdp->widget_paperlist);       n++;
    XtSetArg(args[n], XtNwidth,               DEFAULT_WIDTH);               n++;
    pdp->setup.paperlist = XtCreateManagedWidget("paperlist", listWidgetClass, listform, args, n);

    n = 0;
    XtSetArg(args[n], XtNresizable,           True);                        n++;
    XtSetArg(args[n], XtNforceColumns,        True);                        n++;
    XtSetArg(args[n], XtNdefaultColumns,      1);                           n++;
    XtSetArg(args[n], XtNsensitive,           canChangeResolution);         n++;
    XtSetArg(args[n], XtNlist,                pdp->widget_resolutionlist);  n++;
    XtSetArg(args[n], XtNwidth,               DEFAULT_WIDTH);               n++;
    pdp->setup.resolutionlist = XtCreateManagedWidget("resolutionlist", listWidgetClass, listform, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNresizable,           True);                        n++;
    XtSetArg(args[n], XtNforceColumns,        True);                        n++;
    XtSetArg(args[n], XtNdefaultColumns,      1);                           n++;
    XtSetArg(args[n], XtNsensitive,           canChangeOrientation);        n++;
    XtSetArg(args[n], XtNlist,                pdp->widget_orientationlist); n++;
    XtSetArg(args[n], XtNwidth,               DEFAULT_WIDTH);               n++;
    pdp->setup.orientationlist = XtCreateManagedWidget("orientationlist", listWidgetClass, listform, args, n);

    n = 0;
    XtSetArg(args[n], XtNresizable,           True);                        n++;
    XtSetArg(args[n], XtNforceColumns,        True);                        n++;
    XtSetArg(args[n], XtNdefaultColumns,      1);                           n++;
    XtSetArg(args[n], XtNsensitive,           canChangePlex);               n++;
    XtSetArg(args[n], XtNlist,                pdp->widget_plexlist);        n++;
    XtSetArg(args[n], XtNwidth,               DEFAULT_WIDTH);               n++;
    pdp->setup.plexlist = XtCreateManagedWidget("plexlist", listWidgetClass, listform, args, n);

    n = 0;
    XtSetArg(args[n], XtNresizable,           True);                        n++;
    XtSetArg(args[n], XtNforceColumns,        True);                        n++;
    XtSetArg(args[n], XtNdefaultColumns,      1);                           n++;
    XtSetArg(args[n], XtNsensitive,           True);                        n++;
    XtSetArg(args[n], XtNlist,                pdp->widget_colorspacelist);  n++;
    XtSetArg(args[n], XtNwidth,               DEFAULT_WIDTH);               n++;
    pdp->setup.colorspacelist = XtCreateManagedWidget("colorspacelist", listWidgetClass, listform, args, n);
    
    n = 0;
    XtSetArg(args[n], XtNborderWidth,         0);                           n++;
    XtSetArg(args[n], XtNresizable,           False);                       n++;
    XtSetArg(args[n], XtNjustify,             XtJustifyRight);              n++;
    XtSetArg(args[n], XtNwidth,               DEFAULT_WIDTH);               n++;
    XtSetArg(args[n], XtNsensitive,           canChangeJobCopies);          n++;
    pdp->setup.jobcopieslabel = XtCreateManagedWidget("jobcopieslabel", labelWidgetClass, listform, args, n);

    n = 0;
    XtSetArg(args[n], XtNstring,              "1");                         n++;
    XtSetArg(args[n], XtNresizable,           True);                        n++;
    XtSetArg(args[n], XtNeditType,            XawtextEdit);                 n++;
    XtSetArg(args[n], XtNsensitive,           canChangeJobCopies);          n++;
    pdp->setup.jobcopies = XtCreateManagedWidget("jobcopies", asciiTextWidgetClass, listform, args, n);
    XtAddCallback(pdp->setup.jobcopies, XtNpositionCallback, printSetupJobCopiesXtProc, pdw);

    n = 0;
    pdp->setup.ok = XtCreateManagedWidget("ok", commandWidgetClass, pdp->setup.form, args, n);
    XtAddCallback(pdp->setup.ok, XtNcallback, printSetupOKXtProc, pdw);

    n = 0;
    pdp->setup.cancel = XtCreateManagedWidget("cancel", commandWidgetClass, pdp->setup.form, args, n);
    XtAddCallback(pdp->setup.cancel, XtNcallback, printSetupCancelXtProc, pdw);

    XtRealizeWidget(pdp->setup.popup); 

    return pdp->setup.popup;
}

#undef DEFAULT_WIDTH
#define DEFAULT_WIDTH 150

static
String *xpprinterlist_to_widget_printerlist(XPPrinterList printerlist, int num_printers)
{
    int     i;
    String *names;

    names = malloc(sizeof(String *) * (num_printers+1));
    if(!names)
      return NULL;

    for(i = 0 ; i < num_printers ; i++)
        names[i] = printerlist[i].name;

    names[i] = NULL; /* Terminate the list */

    return names;
}

static
String *xppaperlist_to_widget_paperlist(XpuMediumSourceSizeList paperlist, int num_papers)
{
    int     i;
    String *names;

    names = malloc(sizeof(String *) * (num_papers+1));
    if (!names)
      return NULL;

    for(i = 0 ; i < num_papers ; i++)
        names[i] = (char *)paperlist[i].medium_name; /* FixMe: tray selection missing */

    names[i] = NULL; /* Terminate the list */

    return names;
}

static
String *xpresolutionlist_to_widget_resolutionlist(XpuResolutionList reslist, int num_resolutions)
{
    int     i;
    String *names;

    names = malloc(sizeof(String *) * (num_resolutions+1));
    if (!names)
        return NULL;

    for(i = 0 ; i < num_resolutions ; i++)
        names[i] = (char *)reslist[i].name;

    names[i] = NULL; /* Terminate the list */

    return names;
}

static
String *xporientationlist_to_widget_orientationlist(XpuOrientationList orientationlist, int num_orientations)
{
    int     i;
    String *names;

    names = malloc(sizeof(String *) * (num_orientations+1));
    if (!names)
        return NULL;

    for(i = 0 ; i < num_orientations ; i++)
        names[i] = (char *)orientationlist[i].orientation;

    names[i] = NULL; /* Terminate the list */

    return names;
}

static
String *xpplexlist_to_widget_plexlist(XpuPlexList plexlist, int num_plex)
{
    int     i;
    String *names;

    names = malloc(sizeof(String *) * (num_plex+1));
    if(!names)
      return NULL;

    for(i = 0 ; i < num_plex ; i++)
      names[i] = (char *)plexlist[i].plex;

    names[i] = NULL; /* Terminate the list */

    return names;
}

static
String *xpcolorspacelist_to_widget_colorspacelist(XpuColorspaceList colorspacelist, int num_colorspaces)
{
    int     i;
    String *names;

    names = malloc(sizeof(String *) * (num_colorspaces+1));
    if(!names)
      return NULL;

    for(i = 0 ; i < num_colorspaces ; i++)
      names[i] = (char *)colorspacelist[i].name;

    names[i] = NULL; /* Terminate the list */

    return names;
}

static void
printerSelectionPrinterSelectedXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget    pdw = (PrintDialogWidget)client_data;
    PrintDialogPart     *pdp = &pdw->printdialog;
    XawListReturnStruct *lrs = (XawListReturnStruct *)callData;
    int                  list_index;

    Log(("## user selected a printer\n"));

    list_index = lrs->list_index;
    if (list_index == XAW_LIST_NONE) {
        Log(("printerSelectionPrinterSelectedXtProc: Nothing selected.\n"));
        return;
    }
    
    Log(("selected printer is '%d'/'%s'/'%s'\n", 
        list_index, lrs->string, NULLSTR(pdp->printerlist[list_index].name)));

    /* Enable OK button */
    XtVaSetValues(pdp->selectPrinter.ok, XtNsensitive, True, NULL);
}

static void
printSelectPrinterXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget   pdw = (PrintDialogWidget)client_data;
    PrintDialogPart    *pdp = &pdw->printdialog;

    Log(("## select printer!\n"));

    if (!pdp->selectPrinter.popup) {
        if (!buildPrinterSelectionDialog(pdw)) {
            Log(("printSelectPrinterXtProc: buildPrinterSelectionDialog failure.\n"));
            return;
        }
    }

    center_popup((Widget)pdw, pdp->selectPrinter.popup);

    XtPopup(pdp->selectPrinter.popup, XtGrabExclusive);
}

static void
printerSelectionClose(PrintDialogWidget  pdw)
{ 
    PrintDialogPart *pdp = &pdw->printdialog;

    if (pdp->selectPrinter.popup) {
        XtPopdown(pdp->selectPrinter.popup);
    }
}

static void
printerSelectionOKXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget    pdw = (PrintDialogWidget)client_data;
    PrintDialogPart     *pdp = &pdw->printdialog;
    XawListReturnStruct *lrs;
    int                  list_index;

    Log(("## printerSelectionOKXtProc: closing printer selection!\n"));

    /* Copy current selection in printer list widget and open a printer connection */
    lrs = XawListShowCurrent(pdp->selectPrinter.list);
    if (!lrs) {
        Log(("printerSelectionOKXtProc: |lrs|==NULL\n"));
        return;
    }

    list_index = lrs->list_index;
    if (lrs->list_index == XAW_LIST_NONE) {
        Log(("printerSelectionOKXtProc: rs->list_index == XAW_LIST_NONE\n"));
        XtFree((char *)lrs);
        return;
    }

    /* Set printer name and description in main dialog */
    XtVaSetValues(pdp->main.name, XtNlabel, NULLSTR(pdp->printerlist[list_index].name), NULL);
    XtVaSetValues(pdp->main.desc, XtNlabel, NULLSTR(pdp->printerlist[list_index].desc), NULL);

    if (pdp->printer_name)
        free(pdp->printer_name);
    pdp->printer_name = strdup(lrs->string);

    /* Close any outstanding printer connection since user has selected
     * a different printer */
    closePrinterConnection(pdw, True);

    printerSelectionClose(pdw);
    
    XtFree((char *)lrs);
}

static void
printerSelectionCancelXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    PrintDialogWidget  pdw = (PrintDialogWidget)client_data;
    Log(("## printerSelectionCancelXtProc: closing printer selection!\n"));

    printerSelectionClose(pdw);
}

static
Bool buildPrinterSelectionDialog(PrintDialogWidget  pdw)
{
    PrintDialogPart   *pdp = &pdw->printdialog;

    int  n;
    Arg  args[20];

    n = 0;
    XtSetArg(args[n], XtNallowShellResize, True);                    n++;
    pdp->selectPrinter.popup = XtCreatePopupShell("printerselection", transientShellWidgetClass, (Widget)pdw, args, n);

    n = 0;
    pdp->selectPrinter.form = XtCreateManagedWidget("form", formWidgetClass, pdp->selectPrinter.popup, args, n);

    n = 0;
    XtSetArg(args[n], XtNresizable,        True);                    n++;
    XtSetArg(args[n], XtNforceColumns,     True);                    n++;
    XtSetArg(args[n], XtNdefaultColumns,   1);                       n++;
    XtSetArg(args[n], XtNlist,             pdp->widget_printerlist); n++;
    XtSetArg(args[n], XtNwidth,            DEFAULT_WIDTH);           n++;
    pdp->selectPrinter.list = XtCreateManagedWidget("list", listWidgetClass, pdp->selectPrinter.form, args, n);
    XtAddCallback(pdp->selectPrinter.list, XtNcallback, printerSelectionPrinterSelectedXtProc, pdw);

    n = 0;
    XtSetArg(args[n], XtNsensitive,        False);                   n++;
    pdp->selectPrinter.ok = XtCreateManagedWidget("ok", commandWidgetClass, pdp->selectPrinter.form, args, n);
    XtAddCallback(pdp->selectPrinter.ok, XtNcallback, printerSelectionOKXtProc, pdw);

    n = 0;
    pdp->selectPrinter.cancel = XtCreateManagedWidget("cancel", commandWidgetClass, pdp->selectPrinter.form, args, n);
    XtAddCallback(pdp->selectPrinter.cancel, XtNcallback, printerSelectionCancelXtProc, pdw);

    XtRealizeWidget(pdp->selectPrinter.popup);

    return True;
}

static
void destroyPrinterSelectionDialog(PrintDialogWidget pdw)
{
    PrintDialogPart *pdp = &pdw->printdialog;

    if (pdp->selectPrinter.popup) {
        XtDestroyWidget(pdp->selectPrinter.popup);
    }

    /* |XtDestroyWidget(pdp->setup.popup);| above will recursively destroy
     * all children so we only have to reset the pointers here... */
    pdp->selectPrinter.popup   = NULL;
    pdp->selectPrinter.form    = NULL;
    pdp->selectPrinter.list    = NULL;
    pdp->selectPrinter.popup   = NULL;
    pdp->selectPrinter.cancel  = NULL;
}

/*
 * Function:
 *      createprintdialogchildren
 *
 * Parameters:
 *      w - print dialog widget
 *
 * Description:
 *      Creates the print dialog widgets.
 *
 * Note
 *      -
 */
static void
createprintdialogchildren(Widget w)
{
    int                n;
    Arg                args[20];
    PrintDialogWidget  pdw = (PrintDialogWidget)w;
    PrintDialogPart   *pdp = &pdw->printdialog;
    const char        *default_printername = NULL,
                      *default_printerdesc = NULL;
    Bool               has_default_printer = False;

    pdp->printerlist = XpuGetPrinterList(NULL, &pdp->num_printers);
    pdp->widget_printerlist = xpprinterlist_to_widget_printerlist(pdp->printerlist, pdp->num_printers);

    if (pdp->num_printers > 0) {
        if (pdp->printerlist[0].name && strlen(pdp->printerlist[0].name)) {
            /* XprintUtils moves the default printer to the top */
            default_printername = pdp->printerlist[0].name;
            default_printerdesc = pdp->printerlist[0].desc;
            pdp->printer_name   = strdup(pdp->printerlist[0].name);
            has_default_printer = True;
        }
    }
    else
    {
      PrintMsg(("No Xprint servers could be found.\n"
                "Check whether the XPSERVERLIST environment variable contains any valid Xprint server(s).\n"));
    }
    
    n = 0;
    XtSetArg(args[n], XtNborderWidth,     0);                        n++;
    pdp->main.form = XtCreateManagedWidget("main", formWidgetClass, (Widget)pdw, args, n);

    n = 0;
    pdp->main.innerform = XtCreateManagedWidget("innerform", formWidgetClass, pdp->main.form, args, n);

    n = 0;
    XtSetArg(args[n], XtNborderWidth,     0);                        n++;
    XtSetArg(args[n], XtNresizable,       False);                    n++;
    XtSetArg(args[n], XtNjustify,         XtJustifyRight);           n++;
    XtSetArg(args[n], XtNwidth,           DEFAULT_WIDTH);            n++;
    pdp->main.desclabel = XtCreateManagedWidget("desclabel", labelWidgetClass, pdp->main.innerform, args, n);

    n = 0;
    XtSetArg(args[n], XtNborderWidth,     0);                        n++;
    XtSetArg(args[n], XtNresizable,       False);                    n++;
    XtSetArg(args[n], XtNjustify,         XtJustifyLeft);            n++;
    XtSetArg(args[n], XtNwidth,           DEFAULT_INFOLABEL_WIDTH);  n++;
    XtSetArg(args[n], XtNlabel,           default_printerdesc?default_printerdesc:"-"); n++;
    pdp->main.desc = XtCreateManagedWidget("desc", labelWidgetClass, pdp->main.innerform, args, n);

    n = 0;
    XtSetArg(args[n], XtNsensitive,       has_default_printer);      n++;
    pdp->main.info = XtCreateManagedWidget("info", commandWidgetClass, pdp->main.innerform, args, n);
  /*
    XtAddCallback(pdp->main.info, XtNcallback, printerInfoXtProc, pdw);
  */
    n = 0;
    XtSetArg(args[n], XtNborderWidth,     0);                        n++;
    XtSetArg(args[n], XtNresizable,       False);                    n++;
    XtSetArg(args[n], XtNjustify,         XtJustifyRight);           n++;
    XtSetArg(args[n], XtNwidth,           DEFAULT_WIDTH);            n++;
    pdp->main.namelabel = XtCreateManagedWidget("namelabel", labelWidgetClass, pdp->main.innerform, args, n);

    n = 0;
    XtSetArg(args[n], XtNborderWidth,     0);                        n++;
    XtSetArg(args[n], XtNresizable,       False);                    n++;
    XtSetArg(args[n], XtNjustify,         XtJustifyLeft);            n++;
    XtSetArg(args[n], XtNlabel,           default_printername?default_printername:"-"); n++;
    XtSetArg(args[n], XtNwidth,           DEFAULT_INFOLABEL_WIDTH);  n++;
    pdp->main.name = XtCreateManagedWidget("name", labelWidgetClass, pdp->main.innerform, args, n);

    n = 0;
    pdp->main.selectprinter = XtCreateManagedWidget("selectprinter", commandWidgetClass, pdp->main.innerform, args, n);
    XtAddCallback(pdp->main.selectprinter, XtNcallback, printSelectPrinterXtProc, pdw);

    /* Line: "File selection" */

    n = 0;
    XtSetArg(args[n], XtNborderWidth,     0);                        n++;
    XtSetArg(args[n], XtNresizable,       False);                    n++;
    XtSetArg(args[n], XtNjustify,         XtJustifyRight);           n++;
    XtSetArg(args[n], XtNwidth,           DEFAULT_WIDTH);            n++;
    XtSetArg(args[n], XtNsensitive,       False);                    n++;
    pdp->main.filenamelabel = XtCreateManagedWidget("filenamelabel", labelWidgetClass, pdp->main.innerform, args, n);

    n = 0;
    XtSetArg(args[n], XtNborderWidth,     0);                        n++;
    XtSetArg(args[n], XtNresizable,       False);                    n++;
    XtSetArg(args[n], XtNjustify,         XtJustifyLeft);            n++;
    XtSetArg(args[n], XtNlabel,           "-");                      n++;
    XtSetArg(args[n], XtNwidth,           DEFAULT_INFOLABEL_WIDTH);  n++;
    XtSetArg(args[n], XtNsensitive,       False);                    n++;
    pdp->main.filename = XtCreateManagedWidget("filename", labelWidgetClass, pdp->main.innerform, args, n);

    n = 0;
    XtSetArg(args[n], XtNsensitive,       False);                    n++;
    pdp->main.selectfile = XtCreateManagedWidget("selectfile", commandWidgetClass, pdp->main.innerform, args, n);
    XtAddCallback(pdp->main.selectfile, XtNcallback, printSelectFileXtProc, pdw);

    /* Line: Misc (Print destination toggles, copy-count etc.)*/
    n = 0;
    XtSetArg(args[n], XtNstate,           True);                     n++;
    pdp->main.printtoprinter = XtCreateManagedWidget("printtoprinter", toggleWidgetClass, pdp->main.innerform, args, n);
    XtAddCallback(pdp->main.printtoprinter, XtNcallback, printDialogDestXtProc, pdw);

    n = 0;
    XtSetArg(args[n], XtNstate,           False);                    n++;
    pdp->main.printtofile = XtCreateManagedWidget("printtofile", toggleWidgetClass, pdp->main.innerform, args, n);
    XtAddCallback(pdp->main.printtofile, XtNcallback, printDialogDestXtProc, pdw);

    /* End-of-Inner-Form-Content */

    n = 0;
    XtSetArg(args[n], XtNsensitive,       has_default_printer);       n++;
    pdp->main.ok = XtCreateManagedWidget("ok", commandWidgetClass, pdp->main.form, args, n);
    XtAddCallback(pdp->main.ok, XtNcallback, printOKXtProc, pdw);

    n = 0;
    XtSetArg(args[n], XtNsensitive,       has_default_printer);       n++;
    pdp->main.setup = XtCreateManagedWidget("setup", commandWidgetClass, pdp->main.form, args, n);
    XtAddCallback(pdp->main.setup, XtNcallback, printSetupXtProc, pdw);

    n = 0;
    pdp->main.cancel = XtCreateManagedWidget("cancel", commandWidgetClass, pdp->main.form, args, n);
    XtAddCallback(pdp->main.cancel, XtNcallback, printCancelXtProc, pdw);
}

/*
 * Class Methods
 */
static void    XawPrintDialogConstraintInitialize(Widget, Widget, ArgList, Cardinal *);
static void    XawPrintDialogGetValuesHook(Widget, ArgList, Cardinal*);
static void    XawPrintDialogInitialize(Widget, Widget, ArgList, Cardinal*);
static void    XawPrintDialogDestroy(Widget);
static Boolean XawPrintDialogSetValues(Widget, Widget, Widget, ArgList, Cardinal *);

/*
 * Initialization
 */
static XtResource resources[] = {
  {
    XtNicon,
    XtCIcon,
    XtRBitmap,
    sizeof(Pixmap),
    XtOffsetOf(PrintDialogRec, printdialog.icon),
    XtRImmediate,
    0
  },
  {
    XawNOkCallback, XtCCallback, XtRCallback,
    sizeof(XtCallbackList), XtOffsetOf(PrintDialogRec, printdialog.ok_callback),
    XtRImmediate, (XtPointer)NULL
  },
  {
    XawNCancelCallback, XtCCallback, XtRCallback,
    sizeof(XtCallbackList), XtOffsetOf(PrintDialogRec, printdialog.cancel_callback),
    XtRImmediate, (XtPointer)NULL
  },
};

PrintDialogClassRec printdialogClassRec = {
  /* core */
  {
    (WidgetClass)&formClassRec,         /* superclass */
    "PrintDialog",                      /* class_name */
    sizeof(PrintDialogRec),             /* widget_size */
    XawInitializeWidgetSet,             /* class_initialize */
    NULL,                               /* class_part init */
    False,                              /* class_inited */
    XawPrintDialogInitialize,           /* initialize */
    NULL,                               /* initialize_hook */
    XtInheritRealize,                   /* realize */
    NULL,                               /* actions */
    0,                                  /* num_actions */
    resources,                          /* resources */
    XtNumber(resources),                /* num_resources */
    NULLQUARK,                          /* xrm_class */
    True,                               /* compress_motion */
    True,                               /* compress_exposure */
    True,                               /* compress_enterleave */
    False,                              /* visible_interest */
    XawPrintDialogDestroy,              /* destroy */
    XtInheritResize,                    /* resize */
    XtInheritExpose,                    /* expose */
    XawPrintDialogSetValues,            /* set_values */
    NULL,                               /* set_values_hook */
    XtInheritSetValuesAlmost,           /* set_values_almost */
    XawPrintDialogGetValuesHook,        /* get_values_hook */
    NULL,                               /* accept_focus */
    XtVersion,                          /* version */
    NULL,                               /* callback_private */
    NULL,                               /* tm_table */
    XtInheritQueryGeometry,             /* query_geometry */
    XtInheritDisplayAccelerator,        /* display_accelerator */
    NULL,                               /* extension */
  },
  /* composite */
  {
    XtInheritGeometryManager,           /* geometry_manager */
    XtInheritChangeManaged,             /* change_managed */
    XtInheritInsertChild,               /* insert_child */
    XtInheritDeleteChild,               /* delete_child */
    NULL,                               /* extension */
  },
  /* constraint */
  {
    NULL,                               /* subresourses */
    0,                                  /* subresource_count */
    sizeof(PrintDialogConstraintsRec),  /* constraint_size */
    XawPrintDialogConstraintInitialize, /* initialize */
    NULL,                               /* destroy */
    NULL,                               /* set_values */
    NULL,                               /* extension */
  },
  /* form */
  {
    XtInheritLayout,                    /* layout */
  },
  /* printdialog */
  {
    NULL,                               /* extension */
  }
};

WidgetClass printDialogWidgetClass = (WidgetClass)&printdialogClassRec;

/*
 * Implementation
 */
/*ARGSUSED*/
static void
XawPrintDialogInitialize(Widget request, Widget cnew, ArgList args, Cardinal *num_args)
{
    PrintDialogWidget dw = (PrintDialogWidget)cnew;
    Arg               arglist[9];
    Cardinal          arg_cnt = 0;

    XtSetArg(arglist[arg_cnt], XtNborderWidth, 0);     arg_cnt++;
    XtSetArg(arglist[arg_cnt], XtNleft, XtChainLeft);  arg_cnt++;

    if (dw->printdialog.icon != (Pixmap)0) {
        XtSetArg(arglist[arg_cnt], XtNbitmap, dw->printdialog.icon);     arg_cnt++;
        XtSetArg(arglist[arg_cnt], XtNright, XtChainLeft);               arg_cnt++;
        dw->printdialog.iconW = XtCreateManagedWidget("icon", labelWidgetClass,
                                                      cnew, arglist, arg_cnt);
        arg_cnt = 2;
        XtSetArg(arglist[arg_cnt], XtNfromHoriz, dw->printdialog.iconW); arg_cnt++;
    }
    else {
        dw->printdialog.iconW = NULL;
    }
    
    createprintdialogchildren((Widget)dw);
}

/*ARGSUSED*/
static void
XawPrintDialogConstraintInitialize(Widget request, Widget cnew,
                                   ArgList args, Cardinal *num_args)
{
    PrintDialogWidget dw = (PrintDialogWidget)cnew->core.parent;
    PrintDialogConstraints constraint = (PrintDialogConstraints)cnew->core.constraints;

    if (!XtIsSubclass(cnew, commandWidgetClass)) /* if not a button */
        return;                                  /* then just use defaults */

    constraint->form.left = constraint->form.right = XtChainLeft;
    constraint->form.vert_base = dw->printdialog.main.form;

    if (dw->composite.num_children > 1) {
        WidgetList  children = dw->composite.children;
        Widget     *childP;

        for (childP = children + dw->composite.num_children - 1;
           childP >= children; childP-- ) {
            if (*childP == dw->printdialog.main.form)
                break;
            if (XtIsManaged(*childP) &&
                XtIsSubclass(*childP, commandWidgetClass)) {
                constraint->form.horiz_base = *childP;
                break;
            }
        }
    }
}

#define ICON       0
#define NUM_CHECKS 1
/*ARGSUSED*/
static Boolean
XawPrintDialogSetValues(Widget current, Widget request, Widget cnew,
                        ArgList in_args, Cardinal *in_num_args)
{
    PrintDialogWidget w   = (PrintDialogWidget)cnew;
    PrintDialogWidget old = (PrintDialogWidget)current;
    Arg               args[5];
    unsigned int      i;
    Bool              checks[NUM_CHECKS];

    for (i = 0; i < NUM_CHECKS; i++)
        checks[i] = False;

    for (i = 0; i < *in_num_args; i++) {
        if (!strcmp(XtNicon, in_args[i].name))
            checks[ICON]  = True;
    }

    if (checks[ICON]) {
        if (w->printdialog.icon != 0) {
            XtSetArg(args[0], XtNbitmap, w->printdialog.icon);
            if (old->printdialog.iconW != NULL) {
                XtSetValues(old->printdialog.iconW, args, 1);
            }
            else {
                XtSetArg(args[1], XtNborderWidth, 0);
                XtSetArg(args[2], XtNleft,        XtChainLeft);
                XtSetArg(args[3], XtNright,       XtChainLeft);
                w->printdialog.iconW = XtCreateWidget("icon", labelWidgetClass,
                                                      cnew, args, 4);
                XtManageChild(w->printdialog.iconW);
            }
        }
        else if (old->printdialog.icon != 0) {
            XtDestroyWidget(old->printdialog.iconW);
            w->printdialog.iconW = NULL;
        }
    }

    return True;
}

/*
 * Function:
 *      XawPrintDialogGetValuesHook
 *
 * Parameters:
 *      w        - Print Dialog Widget
 *      args     - argument list
 *      num_args - number of args
 *
 * Description:
 *      This is a get values hook routine that gets the values in the dialog.
 */
static void
XawPrintDialogGetValuesHook(Widget w, ArgList args, Cardinal *num_args)
{
    /* NO-OP for now */
}

static void 
XawPrintDialogDestroy(Widget w)
{
    PrintDialogWidget pdw = (PrintDialogWidget)w;
    Log(("# XawPrintDialogDestroy: start.\n"));

    destroyPrintSetupDialog(pdw);
    destroyPrinterSelectionDialog(pdw);
    closePrinterConnection(pdw, True);
    
    Log(("# XawPrintDialogDestroy: done.\n"));
}

/*
 * Function:
 *      XawPrintDialogClosePrinterConnection
 *
 * Parameters:
 *      w            - Print Dialog Widget
 *      closeDisplay - Boolean which decides whether |Display *|
 *                     and |XPContext| should be disposed, too.
 *
 * Description:
 *      The print display connection is owned by the print dialog
 *      by default.
 *      If the application wishes to close the display connection
 *      to the print server, either to close it itself or force
 *      a disconnection for other reasons this function can be used.
 *
 * Notes
 *      1. PUBLIC API
 *      2. After this function returns all resources returned by the
 *         print dialog which are related to the status of the printer
 *         connection are stale.
 */
void
XawPrintDialogClosePrinterConnection(Widget w, Bool closeDisplay)
{
    closePrinterConnection((PrintDialogWidget)w, closeDisplay);
}

