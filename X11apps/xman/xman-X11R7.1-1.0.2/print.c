/*
 * $Xorg: print.c,v 1.1 2004/04/30 02:05:54 gisburn Exp $
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

/* Turn a NULL pointer string into an empty string */
#define NULLSTR(x) (((x)!=NULL)?(x):(""))

#define Error(x) { printf x ; exit(EXIT_FAILURE); }
#define Assertion(expr, msg) { if (!(expr)) { Error msg } }
#define Log(x)   { if(True) printf x; }

#include "print.h"
#include "ScrollByL.h"
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <stdio.h>
#include <stdlib.h>

static Widget
CreatePrintShell(Widget    videoshell,
                 Screen   *pscreen,
                 Visual   *pvisual,
                 String    printshell_name,
                 ArgList   args,
                 Cardinal  numargs)
{
    String   videoname,
             videoclass;
    Widget   pappshell,
             printshell;
    Display *pdpy = XDisplayOfScreen(pscreen);
    int      dummyc = 0;
    String   dummys = "";
    Cardinal shell_n;
    Arg      shell_args[5];

    XtGetApplicationNameAndClass(XtDisplay(videoshell), 
                                 &videoname, &videoclass);

    /* XXX: Why is the |dummyc|&&|dummys| stuff needed here ? */
    XtDisplayInitialize(XtWidgetToApplicationContext(videoshell), pdpy,
                        videoname, videoclass,
                        NULL, 0,
                        &dummyc, &dummys);

    shell_n = 0;
    XtSetArg(shell_args[shell_n], XtNscreen, pscreen); shell_n++;
    if (pvisual) {
        XtSetArg(shell_args[shell_n], XtNvisual, pvisual); shell_n++;
    }
    pappshell = XtAppCreateShell(videoname, videoclass,
                                   applicationShellWidgetClass,
                                   pdpy,
                                   shell_args, shell_n);
    printshell = XtCreatePopupShell(printshell_name,
                                    xawPrintShellWidgetClass,
                                    pappshell, args, numargs);
                                

    /* we're mapping/unmapping at start/end page time */
    XtSetMappedWhenManaged(printshell, False);

    /* We realise the widget when we're done with building the widget tree... */
    
    return printshell;
}

typedef struct
{
  const char       *programname;
  Widget            toplevel;
  Bool              isPrinting;
  Widget            printshell;
  struct
  {
      Widget  form;
      Widget  pageheaderlabel;
      Widget  manpage;
  } content; /* content to print */
  int               numpages;
  Display          *pdpy;
  Screen           *pscreen;
  XPContext         pcontext;
  XtCallbackProc    pdpyDestroyCallback;
  void             *printtofile_handle;
  const char       *jobtitle;
} AppPrintData;

static AppPrintData  apdx;
static AppPrintData *apd = &apdx;

/* "Count" pages in a scrollByLineWidgetClass widget */
static
long CountPages(Widget sblw)
{
    long     num_pages = 0;
    Cardinal n;
    Arg      args[3];
    int      num_total_lines   = 0,
             num_visible_lines = 0;
    
    n = 0;
    XtSetArg(args[n], XtNNumTotalLines,   &num_total_lines);   n++;
    XtSetArg(args[n], XtNNumVisibleLines, &num_visible_lines); n++;
    XtGetValues(sblw, args, n);
    
#define DIV_ROUND_UP(a, b) (((a)+((b)-1)) / (b))
    /* scrollByLineWidgetClass's "Page(forward)" always overlaps by one
     * line so we use |num_visible_lines-1| instead of |num_visible_lines| */
    num_pages = DIV_ROUND_UP(num_total_lines, num_visible_lines-1);
#undef DIV_ROUND_UP

    return num_pages;
}

static void 
PageSetupCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    Widget                       pshell = widget;
    XawPrintShellCallbackStruct *psp    = (XawPrintShellCallbackStruct *)call_data;
    AppPrintData                *p      = (AppPrintData *)client_data;
   
    Log(("--> PageSetupCB\n"));

    if (!psp->last_page_in_job) {
        int  currpage;
        char buffer[256];

        XtVaGetValues(pshell, XawNcurrPageNumInJob, &currpage, NULL);

        sprintf(buffer, "Title: %s / Page: %d/%d", p->jobtitle, currpage, p->numpages);
        XtVaSetValues(apd->content.pageheaderlabel, XtNlabel, buffer, NULL);

        /* Note: XawPrintShell's pagecount starts with '1'
         * (=first page is page no. '1') */
        if (currpage > 1) {
            String params[] = { "Forward" };
            Log(("pagedown %d\n", currpage));
            /* "Page(Forward)" is scrollByLineWidgetClass's way to
             * move one page forward */
            XtCallActionProc(p->content.manpage, "Page", NULL, params, ONE);
        }
        else
        {
            Log(("first page\n"));
        }              
      
        if (currpage >= p->numpages) {
            psp->last_page_in_job = True;
        }
    }
}

static
void FinishPrinting(AppPrintData *p)
{
    /* Wait for the job to finish */
    if (p->printtofile_handle) {
        if (XpuWaitForPrintFileChild(p->printtofile_handle) != XPGetDocFinished) {
            fprintf(stderr, "%s: Error while printing to file.\n", apd->programname);
        }
        p->printtofile_handle = NULL;
    }   

    /* Avoid that the manpage object tries to close the |FILE *|-handle at destruction time */
    XtVaSetValues(p->content.manpage, XtNfile, NULL, NULL);

    if (p->printshell) {
        XtDestroyWidget(p->printshell);
        p->printshell = NULL;
    }
  
    /* Two issues here:
     * 1. The print display connection is owned by the print dialog
     *    To avoid any problems with that use a callback back to the main
     *    application which calls
     *    |XawPrintDialogClosePrinterConnection(w, False)| to ask the
     *    print dialog widget to close all print display resources and
     *    disown the object.
     * 2. We have to use XpDestroyContext() and XtCloseDisplay()
     *    instead of XpuClosePrinterDisplay() to make libXt happy...
     *
     * Call callback... */
    (*apd->pdpyDestroyCallback)(p->toplevel, NULL, NULL); /* HACK! */

    /* ... and then get rid of the display */
    if (p->pcontext != None) {
        XpDestroyContext(p->pdpy, p->pcontext);
    }
    XtCloseDisplay(p->pdpy);

    p->toplevel   = NULL;
    p->isPrinting = False;
    p->pdpy       = NULL;
    p->pscreen    = NULL;
    p->pcontext   = None;
}

static
void PrintEndJobCB(Widget pshell, XtPointer client_data, XtPointer call_data)
{
    AppPrintData *p = (AppPrintData *)client_data;

    Log(("--> PrintEndJobCB\n"));

    /* Finish printing and destroy print shell (it's legal to destroy Xt
     * widgets from within it's own callbacks) */
    FinishPrinting(p);
}

static
XFontStruct *GetPrintTextFont(Display *pdpy, const char *fontprefix, long dpi_x, long dpi_y)
{
    XFontStruct *font;
    char         fontname[1024];

    sprintf(fontname, "%s--*-120-%ld-%ld-*-*-iso8859-1", fontprefix, dpi_x, dpi_y);
    font = XLoadQueryFont(pdpy, fontname);
    if (!font) {    
        sprintf(fontname, "-adobe-courier-medium-r-normal--*-120-%ld-%ld-*-*-iso8859-1", dpi_x, dpi_y);
        font = XLoadQueryFont(pdpy, fontname);
    }
    if (!font) {          
        sprintf(fontname, "-*-*-*-*-*-*-*-120-%ld-%ld-*-*-iso8859-1", dpi_x, dpi_y);
        font = XLoadQueryFont(pdpy, fontname);
    }
    if (!font)
        Error(("XLoadQueryFont failure.\n"));
    return font;

}

void DoPrintManpage(const char *programname,
                    FILE *manpagefile, Widget toplevel,
                    Display *pdpy, XPContext pcontext,
                    XpuColorspaceRec *colorspace,
                    XtCallbackProc pdpyDestroyCB,
                    const char *jobtitle, const char *toFile)
{
    long               dpi_x = 0L,
                       dpi_y = 0L;
    int                n;
    Arg                args[20];
    XFontStruct       *printFontNormal;
    XFontStruct       *printFontBold;
    XFontStruct       *printFontItalic;
    XFontStruct       *printFontSymbol;

    if (!manpagefile) {
        Error(("DoPrintManpage: No FILE given."));
    }
       
    apd->programname         = programname;
    apd->pdpyDestroyCallback = pdpyDestroyCB;
  
    if (apd->isPrinting) {
        fprintf(stderr, "%s: Already busy with printing.\n", apd->programname);
        return;
    } 
        
    /* Configure the print context (paper size, title etc.)
     * We must do this before creating any Xt widgets - otherwise they will
     * make wrong assuptions about fonts, resultions etc. ...
     */
    XpuSetJobTitle(pdpy, pcontext, jobtitle);
    
    /* Configuration done, set the context */
    XpSetContext(pdpy, pcontext);   

    /* Get default printer resolution */   
    if (XpuGetResolution(pdpy, pcontext, &dpi_x, &dpi_y) != 1) {
        fprintf(stderr, "%s: No default resolution for printer.\n", apd->programname);
        XpuClosePrinterDisplay(pdpy, pcontext);
        return;
    }
    
    apd->toplevel = toplevel;
    apd->pdpy     = pdpy;
    apd->pcontext = pcontext;
    apd->pscreen  = XpGetScreenOfContext(pdpy, pcontext);
    apd->jobtitle = jobtitle;

    n = 0;
    /* Override any geometry resource settings as XawPrintShell adjusts it's size
     * to the current page siue when |XawPrintLAYOUTMODE_DRAWABLEAREA| or
     * |XawPrintLAYOUTMODE_PAGESIZE| are used. */
    XtSetArg(args[n], XtNgeometry,    "+0+0");                          n++;
    XtSetArg(args[n], XawNlayoutMode, XawPrintLAYOUTMODE_DRAWABLEAREA); n++;
    if (colorspace) {
        printf("Setting visual to id=0x%lx.\n", colorspace->visualinfo.visualid);
    }
    apd->printshell = CreatePrintShell(toplevel, apd->pscreen, 
                                       (colorspace?(colorspace->visualinfo.visual):(NULL)),
                                       "printshell", args, n);

    n = 0;
    XtSetArg(args[n], XtNresizable,            True);            n++;
    XtSetArg(args[n], XtNright,                XtChainRight);    n++;
    apd->content.form = XtCreateManagedWidget("form", formWidgetClass, apd->printshell, args, n);

    printFontNormal = GetPrintTextFont(pdpy, "-*-courier-medium-r-*", dpi_x, dpi_y);
    printFontBold   = GetPrintTextFont(pdpy, "-*-courier-bold-r-*",   dpi_x, dpi_y);
    printFontItalic = GetPrintTextFont(pdpy, "-*-courier-medium-o-*", dpi_x, dpi_y);
    printFontSymbol = GetPrintTextFont(pdpy, "-*-symbol-*-*-*",       dpi_x, dpi_y);

    n = 0;
    XtSetArg(args[n], XtNfromHoriz,            NULL);            n++;
    XtSetArg(args[n], XtNfromVert,             NULL);            n++;
    XtSetArg(args[n], XtNtop,                  XtChainTop);      n++;
    XtSetArg(args[n], XtNright,                XtChainRight);    n++;
    XtSetArg(args[n], XtNresizable,            True);            n++;
    XtSetArg(args[n], XtNfont,                 printFontNormal); n++; /* fontset would be better */
    XtSetArg(args[n], XtNlabel,                "Page: n/n");     n++;
    XtSetArg(args[n], XtNjustify,              XtJustifyRight);  n++;
    apd->content.pageheaderlabel = XtCreateManagedWidget("pageinfo", labelWidgetClass, apd->content.form, args, n);

    n = 0;
    XtSetArg(args[n], XtNfile,             manpagefile);                  n++;

/* Usually I would expect that using |XtNfromVert, apd->content.pageheaderlabel|
 * would be the correct way to place the text widget with the main content below
 * the page header widget - but for an unknown reason this doesn not work: The
 * text widget squishes itself into the bottom half of the page and only occupies
 * 1/2 of the page's with... ;-(( */
#define WORKAROUND_FOR_SOMETHING_IS_WRONG 1
#ifdef WORKAROUND_FOR_SOMETHING_IS_WRONG
    XtSetArg(args[n], XtNtop,              XtChainTop);                   n++;
    XtSetArg(args[n], XtNright,            XtChainRight);                 n++;
    XtSetArg(args[n], XtNvertDistance,     (printFontNormal->ascent+printFontNormal->descent+2)*2); n++;    
#else
    XtSetArg(args[n], XtNfromHoriz,        NULL);                         n++;
    XtSetArg(args[n], XtNfromVert,         apd->content.pageheaderlabel); n++;
#endif

    XtSetArg(args[n], XtNmanualFontNormal, printFontNormal);              n++;
    XtSetArg(args[n], XtNmanualFontBold,   printFontBold);                n++;
    XtSetArg(args[n], XtNmanualFontItalic, printFontItalic);              n++;
    XtSetArg(args[n], XtNmanualFontSymbol, printFontSymbol);              n++;
    apd->content.manpage = XtCreateManagedWidget("manpage", scrollByLineWidgetClass, apd->content.form, args, n);
   
    XtAddCallback(apd->printshell, XawNpageSetupCallback, PageSetupCB,   (XtPointer)apd);
    XtAddCallback(apd->printshell, XawNendJobCallback,    PrintEndJobCB, (XtPointer)apd);

    /* Realise print shell (which will set position+size of the child
     * widgets based on the current page size) */
    XtRealizeWidget(apd->printshell);
    
    /* Count number of pages in the manpage widget */
    apd->numpages = CountPages(apd->content.manpage);

    /* Make sure that the Xt machinery is really using the right screen (assertion) */
    if (XpGetScreenOfContext(XtDisplay(apd->printshell), apd->pcontext) != XtScreen(apd->printshell))
        Error(("Widget's screen != print screen. BAD.\n"));

    apd->isPrinting = True;

    if (toFile) {
        printf("%s: Printing to file '%s'...\n", apd->programname, toFile);
        apd->printtofile_handle = XpuStartJobToFile(pdpy, pcontext, toFile);
        if (!apd->printtofile_handle) {
            perror("XpuStartJobToFile failure");
            apd->isPrinting = False;
            return;
        }
    }
    else
    {
        printf("%s: Printing to printer...\n", apd->programname);
        XpuStartJobToSpooler(pdpy);
    }
}


