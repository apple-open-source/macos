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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "xlogo.h"
#include "Logo.h"
#include "print.h"
#include <stdio.h>
#include <stdlib.h>

static Widget
CreatePrintShell(Widget    videoshell,
                 Screen   *pscreen, 
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
    XtGetApplicationNameAndClass(XtDisplay(videoshell), 
                                 &videoname, &videoclass);

    /* XXX: Why is the |dummyc|&&|dummys| stuff needed here ? */
    XtDisplayInitialize(XtWidgetToApplicationContext(videoshell), pdpy,
                        videoname, videoclass,
                        NULL, 0,
                        &dummyc, &dummys);

    pappshell = XtVaAppCreateShell(videoname, videoclass,
                                   applicationShellWidgetClass,
                                   pdpy,
                                   XtNscreen, pscreen,
                                   NULL);
    printshell = XtCreatePopupShell(printshell_name,
                                    xawPrintShellWidgetClass,
                                    pappshell, args, numargs);
                                

    /* we're mapping/unmapping at start/end page time */
    XtSetMappedWhenManaged(printshell, False);

    XtRealizeWidget(printshell);
    
    return printshell;
}

typedef struct
{
  Widget      toplevel;
  Bool        isPrinting;
  Widget      printshell;
  Display    *pdpy;
  Screen     *pscreen;
  XPContext   pcontext;
  void       *printtofile_handle;
} AppPrintData;

static AppPrintData  apdx;
static AppPrintData *apd = &apdx;

static void 
PageSetupCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    Widget                       pshell = widget;
    XawPrintShellCallbackStruct *psp    = (XawPrintShellCallbackStruct *)call_data;
    AppPrintData                *p      = (AppPrintData *)client_data;

    Log(("--> PageSetupCB\n"));

    if (!psp->last_page_in_job) {
        Widget plogo;
        Log(("Creating print logo\n"));
        plogo = XtCreateManagedWidget("xlogo", logoWidgetClass, pshell, NULL, ZERO);

        /* Make sure that the Xt machinery is really using the right screen (assertion) */
        if (XpGetScreenOfContext(XtDisplay(plogo), p->pcontext) != XtScreen(plogo))
            Error(("Widget's screen != print screen. BAD.\n"));
        
        /* XLogo always only prints one page */
        psp->last_page_in_job = True;
    }
}

void FinishPrinting(AppPrintData *p)
{
    char *scr;

    if (p->printtofile_handle) {
        if (XpuWaitForPrintFileChild(p->printtofile_handle) != XPGetDocFinished) {
            fprintf(stderr, "%s: Error while printing to file.\n", ProgramName);
        }
        p->printtofile_handle = NULL;
    }   

    /* Job completed, check if there are any messages from the spooler command */
    scr = XpGetOneAttribute(p->pdpy, p->pcontext, XPJobAttr, "xp-spooler-command-results");
    if( scr )
    {
      if( strlen(scr) > 0 )
      {
        const char *msg = XpuCompoundTextToXmb(p->pdpy, scr);
        if( msg )
        {
          Msg(("Spooler command returned '%s'.\n", msg));
          XpuFreeXmbString(msg);
        }
        else
        {
          Msg(("Spooler command returned '%s' (unconverted).\n", scr));
        }
      }

      XFree((void *)scr);
    }

    if (p->printshell) {
        XtDestroyWidget(p->printshell);
        p->printshell = NULL;
    }
  
    if (p->pdpy) {
      /* We have to use XpDestroyContext() and XtCloseDisplay() instead
       * of XpuClosePrinterDisplay() to make libXt happy... */
      if (p->pcontext != None)
          XpDestroyContext(p->pdpy, p->pcontext);
      XtCloseDisplay(p->pdpy);
    }

    /* "Print once and exit"-mode ? */
    if (userOptions.printAndExit) {
        XtAppSetExitFlag(XtWidgetToApplicationContext(p->toplevel));
    }

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

void DoPrint(Widget toplevel, const char *printername, const char *toFile)
{
    XPPrinterList  plist;               /* list of printers */
    int            plist_count;         /* number of entries in |plist|-array */
    Display       *pdpy        = NULL;
    XPContext      pcontext    = None;
    long           dpi_x       = 0L,
                   dpi_y       = 0L;

    if (apd->isPrinting) {
        fprintf(stderr, "%s: Already busy with printing.\n", ProgramName);
        return;
    }
      
    plist = XpuGetPrinterList(printername, &plist_count);
    if (!plist) {
        fprintf(stderr, "%s:  no printers found for printer spec \"%s\".\n",
                ProgramName, NULLSTR(printername));
        return;
    }
    
    printername = strdup(plist[0].name);
    XpuFreePrinterList(plist);
    
    Log(("Using printer '%s'\n", printername));

    /* Get printer, either by "name" (foobar) or "name@display" (foobar@gaja:5) */
    if( XpuGetPrinter(printername, &pdpy, &pcontext) != 1 ) {
        fprintf(stderr, "%s: Cannot open printer '%s'.\n", ProgramName, printername);
        return;
    }
        
    /* Configure the print context (paper size, title etc.)
     * We must do this before creating any Xt widgets - otherwise they will
     * make wrong assuptions about fonts, resultions etc. ...
     */
    XpuSetJobTitle(pdpy, pcontext, "Xprint XLogo");
    
    /* Configuration done, set the context */
    XpSetContext(pdpy, pcontext);   

    /* Get default printer resolution */   
    if (XpuGetResolution(pdpy, pcontext, &dpi_x, &dpi_y) != 1) {
        fprintf(stderr, "%s: No default resolution for printer '%s'\n", ProgramName, printername);
        XpuClosePrinterDisplay(pdpy, pcontext);
        return;
    }
    
    apd->toplevel = toplevel;
    apd->pdpy     = pdpy;
    apd->pcontext = pcontext;
    apd->pscreen  = XpGetScreenOfContext(pdpy, pcontext);

    apd->printshell = CreatePrintShell(toplevel, apd->pscreen, "Print", NULL, 0);
    
    XtAddCallback(apd->printshell, XawNpageSetupCallback, PageSetupCB,   (XtPointer)apd);
    XtAddCallback(apd->printshell, XawNendJobCallback,    PrintEndJobCB, (XtPointer)apd);

    apd->isPrinting = True;

    if (toFile) {
        printf("%s: Printing to file '%s'...\n", ProgramName, toFile);
        apd->printtofile_handle = XpuStartJobToFile(pdpy, pcontext, toFile);
        if (!apd->printtofile_handle) {
            perror("XpuStartJobToFile failure");
            apd->isPrinting = False;
            return;
        }
    }
    else
    {
        printf("%s: Printing to printer '%s'...\n", ProgramName, printername);
        XpuStartJobToSpooler(pdpy);
    }
}


