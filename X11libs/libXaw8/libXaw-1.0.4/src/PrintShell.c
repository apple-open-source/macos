/* $Xorg: PrintShell.c,v 1.1 2003/07/11 19:46:06 gisburn Exp $ */
/******************************************************************************
 ******************************************************************************
 **
 ** (c) Copyright 2003 Danny Backx <danny.backx@skynet.be>
 ** (c) Copyright 2003-2004 Roland Mainz <roland.mainz@nrubsig.org>
 ** 
 ** Permission is hereby granted, free of charge, to any person obtaining a copy
 ** of this software and associated documentation files (the "Software"), to deal
 ** in the Software without restriction, including without limitation the rights
 ** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 ** copies of the Software, and to permit persons to whom the Software is
 ** furnished to do so, subject to the following conditions:
 **
 ** The above copyright notice and this permission notice shall be included in
 ** all copies or substantial portions of the Software.
 **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 ** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 ** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 ** COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 ** IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 ** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **
 ** Except as contained in this notice, the names of the copyright holders shall
 ** not be used in advertising or otherwise to promote the sale, use or other
 ** dealings in this Software without prior written authorization from said
 ** copyright holders.
 **
 ******************************************************************************
 *****************************************************************************/
 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>

#include "Print.h"
#include "PrintSP.h"

/* Local prototypes */
static void    class_initialize(void);
static void    class_part_initialize(WidgetClass w_class);
static void    initialize(Widget request, Widget new_w, ArgList args, Cardinal *num_args);
static void    destroy(Widget w);
static Boolean set_values(Widget current, Widget request, Widget new_w, ArgList args, Cardinal *num_args);
static void    XawPrintNotify(Widget w, XtPointer client, XEvent *evp, Boolean *cont);
static void    XawAttributesNotify(Widget w, XtPointer client, XEvent *evp, Boolean *cont);
static void    XawUpdateLayout(Widget w);
static void    XawUpdateResources(Widget w, XPContext pcontext);

#define Offset(field) XtOffsetOf(XawPrintShellRec, print.field)

#ifdef XAWDEBUG
#define DEBUGOUT(x) XawDebug x ;

static void
XawDebug(const char *fn, Widget w, const char *fmt, ...)
{
    va_list ap;

    if (w) {
        fprintf(stderr, "%s %s: ",
                w->core.widget_class->core_class.class_name, XtName(w));
    } else {
        fprintf(stderr, "(null widget): ");
    }

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
#else
#define DEBUGOUT(x)
#endif /* XAWDEBUG */

/* Resources for the PrintShell class */
static XtResource resources[] =
{
  {
    XawNstartJobCallback, XtCCallback, XtRCallback,
    sizeof(XtCallbackList), Offset(start_job_callback),
    XtRImmediate, (XtPointer)NULL
  },
  {
    XawNendJobCallback, XtCCallback, XtRCallback,
    sizeof(XtCallbackList), Offset(end_job_callback),
    XtRImmediate, (XtPointer)NULL
  },
  {
    XawNdocSetupCallback, XtCCallback, XtRCallback,
    sizeof(XtCallbackList), Offset(doc_setup_callback),
    XtRImmediate, (XtPointer)NULL
  },
  {
    XawNpageSetupCallback, XtCCallback, XtRCallback,
    sizeof(XtCallbackList), Offset(page_setup_callback),
    XtRImmediate, (XtPointer)NULL
  },
  {
    XawNlayoutMode, XawCLayoutMode, XtREnum,
    sizeof(XtEnum), Offset(layoutmode),
    XtRImmediate, (XtPointer)XawPrintLAYOUTMODE_PAGESIZE
  },
  {
    XawNminX, XawCMinX, XtRDimension,
    sizeof(Dimension), Offset(min_x),
    XtRImmediate, (XtPointer)NULL        /* dynamic */
  },
  {
    XawNminY, XawCMinY, XtRDimension,
    sizeof(Dimension), Offset(min_y),
    XtRImmediate, (XtPointer)NULL        /* dynamic */
  },
  {
    XawNmaxX, XawCMaxX, XtRDimension,
    sizeof(Dimension), Offset(max_x),
    XtRImmediate, (XtPointer)NULL        /* dynamic */
  },
  {
    XawNmaxY, XawCMaxY, XtRDimension,
    sizeof(Dimension), Offset(max_y),
    XtRImmediate, (XtPointer)NULL        /* dynamic */
  },
  {
    XawNcurrDocNumInJob, XawCCurrDocNumInJob, XtRInt,
    sizeof(unsigned int), Offset(curr_doc_num_in_job),
    XtRImmediate, (XtPointer)NULL        /* dynamic */
  },
  {
    XawNcurrPageNumInDoc, XawCCurrPageNumInDoc, XtRInt,
    sizeof(unsigned int), Offset(curr_page_num_in_doc),
    XtRImmediate, (XtPointer)NULL        /* dynamic */
  },
  {
    XawNcurrPageNumInJob, XawCCurrPageNumInJob, XtRInt,
    sizeof(unsigned int), Offset(curr_page_num_in_job),
    XtRImmediate, (XtPointer)NULL        /* dynamic */
  },
  {
    XawNdefaultPixmapResolution, XawCDefaultPixmapResolution, XtRShort,
    sizeof(unsigned short), Offset(default_pixmap_resolution),
    XtRImmediate, (XtPointer)0
  },
};

static XtActionsRec actions[] =
{
    { NULL, NULL }
};

XawPrintShellClassRec xawPrintShellClassRec = {
    /* Core class part */
    {
    /* superclass            */ (WidgetClass) &applicationShellClassRec,
    /* class_name            */ "XawPrintShell",
    /* widget_size           */ sizeof(XawPrintShellRec),
    /* class_initialize      */ class_initialize,
    /* class_part_initialize */ class_part_initialize,
    /* class_inited          */ False,
    /* initialize            */ initialize,
    /* initialize_hook       */ NULL,
    /* realize               */ XtInheritRealize,
    /* actions               */ actions,
    /* num_actions           */ XtNumber(actions),
    /* resources             */ resources,
    /* num_resources         */ XtNumber(resources),
    /* xrm_class             */ NULLQUARK,
    /* compress_motion       */ False,
    /* compress_exposure     */ XtExposeCompressSeries,
    /* compress_enterleave   */ False,
    /* visible_interest      */ False,
    /* destroy               */ destroy,
    /* resize                */ XtInheritResize,
    /* expose                */ XtInheritExpose,
    /* set_values            */ set_values,
    /* set_values_hook       */ NULL,
    /* set_values_almost     */ XtInheritSetValuesAlmost,
    /* get_values_hook       */ NULL,
    /* accept_focus          */ NULL,
    /* version               */ XtVersion,
    /* callback offsets      */ NULL,
    /* tm_table              */ XtInheritTranslations,
    /* query_geometry        */ XtInheritQueryGeometry,
    /* display_accelerator   */ NULL,
    /* extension             */ NULL /* (XtPointer)&_XawPrintShellCoreClassExtRec */
    },
    /* Composite class part */
    {
    /* geometry manager */    XtInheritGeometryManager,
    /* change_managed   */    XtInheritChangeManaged,
    /* insert_child     */    XtInheritInsertChild,
    /* delete_child     */    XtInheritDeleteChild,
    /* extension        */    NULL,    
    },
    /* Shell class part */
    {
    /* extension        */    NULL,
    },
    /* WM Shell class part */
    {
    /* extension    */        NULL,
    },
    /* VendorShell class part */
    {
    /* extension    */        NULL,
    },
    /* TopLevelShell class part */
    {
    /* extension    */        NULL,
    },
    /* ApplicationShell class part */
    {
    /* extension    */        NULL,
    },
    {
        /* ?? */              NULL,
    },
};

WidgetClass xawPrintShellWidgetClass = (WidgetClass)&xawPrintShellClassRec;


static void
class_initialize(void)
{
}


static void
class_part_initialize(WidgetClass widget_class)
{
}

/*
 * This is a static table to keep the link between widgets and XPContexts.
 * Yeah - this is probably not a very bright idea. Maybe it should also
 * contain the Display.
 */
typedef struct {
    Widget    w;
    XPContext c;
} WidgetContext;
static WidgetContext *w_ctxt     = NULL;
static int            wc_nfields = 0;

static void
XawStoreWidgetContext(Widget w, XPContext c)
{
    wc_nfields++;
    w_ctxt = (WidgetContext *)XtRealloc((XtPointer)w_ctxt, sizeof(WidgetContext) * wc_nfields);
    w_ctxt[wc_nfields-1].w = w;
    w_ctxt[wc_nfields-1].c = c;
}

/* FIXME: This is not threadsafe... */
static Widget
XawPrintContextToWidget(XPContext c)
{
    int i;

    for( i = 0 ; i < wc_nfields ; i++ ) {
        if( w_ctxt[i].c == c ) {
            return w_ctxt[i].w;
        }
    }
    return NULL;
}

/* FIXME: This is not threadsafe... */
static XPContext
XawPrintWidgetToContext(Widget w)
{
    int i;

    for( i = 0 ; i < wc_nfields ; i++ ) {
        if (w_ctxt[i].w == w) {
            return w_ctxt[i].c;
        }
    }
    return (XPContext)None;
}

/* FIXME: This is not threadsafe... */
static void
XawPrintDeleteWidgetContext(Widget w)
{
    int i;

    for( i = 0 ; i < wc_nfields ; i++ ) {
        if( w_ctxt[i].w == w ) {
            w_ctxt[i].w = NULL;
            w_ctxt[i].c = None;
        }
    }
}

static void
SelectNotify(Widget w, int *e, XtPointer *s, int n, XtPointer client)
{
    XPContext c = XpGetContext(XtDisplay(w));

    if (!c) {
        XtAppWarning(XtWidgetToApplicationContext(w),
                     "XawPrintShell: SelectNotify: no print context\n");
        return;
    }

    XpSelectInput(XtDisplay(w), c, XPPrintMask|XPAttributeMask);   
}


static Boolean
DispatchEvent(XEvent *evp)
{
    XPPrintEvent *e = (XPPrintEvent*)evp;

    Widget w = XawPrintContextToWidget(e->context);

    /* Make sure this event is really for this window... */
    if (XFilterEvent(evp, XtWindow(w)))
    {
        DEBUGOUT((__FILE__, w, "XawPrintShell-DispatchEvent *** filter XFilterEvent() matched.\n"));
        return True;
    }

    /* Only for debugging */   
#ifdef XAWDEBUG
    {
        int error_base,
            event_base;

        if (!XpQueryExtension(XtDisplay(w), &event_base, &error_base)) {
            return False;
        }

        if (e->type == event_base + XPPrintNotify) {
            switch (e->detail) {
            case XPStartJobNotify:
                DEBUGOUT((__FILE__, w, "XawPrintShell-DispatchEvent XPStartJobNotify\n"));
                break;
            case XPEndJobNotify:
                DEBUGOUT((__FILE__, w, "XawPrintShell-DispatchEvent XPEndJobNotify\n"));
                break;
            case XPStartDocNotify:
                DEBUGOUT((__FILE__, w, "XawPrintShell-DispatchEvent XPStartDocNotify\n"));
                break;
            case XPStartPageNotify:
                DEBUGOUT((__FILE__, w, "XawPrintShell-DispatchEvent XPStartPageNotify\n"));
                break;
            case XPEndPageNotify:
                DEBUGOUT((__FILE__, w, "XawPrintShell-DispatchEvent XPEndPageNotify\n"));
                break;
            case XPEndDocNotify:
                DEBUGOUT((__FILE__, w, "XawPrintShell-DispatchEvent XPEndDocNotify\n"));
                break;
            default:
                DEBUGOUT((__FILE__, w, "XawPrintShell DispatchEvent\n"));
            }
        }
    }
#endif /* XAWDEBUG */

    return XtDispatchEventToWidget(w, evp);
}


static void
initialize(Widget request, Widget new_w, ArgList args, Cardinal *num_args)
{
    int       error_base,
              event_base;
    XPContext pcontext;

    DEBUGOUT((__FILE__, new_w, "XawPrintShell Initialize\n"));

    if (!XpQueryExtension(XtDisplay(new_w), &event_base, &error_base)) {
        DEBUGOUT((__FILE__, new_w, "XawPrintShell initialize: failed!!\n"));
        XtAppWarning(XtWidgetToApplicationContext(new_w),
                     "XawPrintShell: initialize: XpQueryExtension() failed. BAD.\n");
        return;
    }

    DEBUGOUT((__FILE__, new_w, "XawPrintShell Initialize event_base %d error_base %d\n",
                      event_base, error_base));

    pcontext = XpGetContext(XtDisplay(new_w));

    if( pcontext == None ) {
        XtAppWarning(XtWidgetToApplicationContext(new_w),
                     "XawPrintShell: initialize: No print content. BAD.\n");
        return;
    }
    
    /* Make sure that the Xt machinery is really using the right screen (assertion) */
    if( XpGetScreenOfContext(XtDisplay(new_w), pcontext) != XtScreen(new_w) ) {
        XtAppWarning(XtWidgetToApplicationContext(new_w),
                     "XawPrintShell: initialize: Widget's screen != print screen. BAD.\n");
        return;
    }

    XawStoreWidgetContext(new_w, pcontext);

    XtInsertEventTypeHandler(new_w,
                             event_base + XPPrintNotify,
                             (XtPointer)XPPrintMask,
                             XawPrintNotify, NULL,
                             XtListTail);
    XtInsertEventTypeHandler(new_w,
                             event_base + XPAttributeNotify,
                             (XtPointer)XPAttributeMask, 
                             XawAttributesNotify, NULL,
                             XtListTail);

    XtRegisterExtensionSelector(XtDisplay(new_w),
                                event_base + XPPrintNotify,
                                event_base + XPAttributeNotify,
                                SelectNotify,
                                NULL);

    XtSetEventDispatcher(XtDisplay(new_w),
                         event_base + XPPrintNotify,
                         DispatchEvent);
    XtSetEventDispatcher(XtDisplay(new_w),
                         event_base + XPAttributeNotify,
                         DispatchEvent);

    PS_LastPageInDoc(new_w) = False;
    PS_LastPageInJob(new_w) = False;

    XawUpdateResources(new_w, pcontext);
    XawUpdateLayout(new_w);
    
    DEBUGOUT((__FILE__, new_w, "XawPrintShell Initialize x %d y %d wid %d ht %d\n",
             new_w->core.x,
             new_w->core.y,
             new_w->core.width,
             new_w->core.height));
}


static void
destroy(Widget w)
{
    DEBUGOUT((__FILE__, w, "XawPrintShell Destroy\n"));
    XawPrintDeleteWidgetContext(w);
}

static Boolean
set_values(Widget current, Widget request, Widget new_w,
           ArgList args, Cardinal *num_args)
{
    DEBUGOUT((__FILE__, new_w, "XawPrintShell SetValues\n"));
    return True;
}

void XawPrintRedisplayWidget(Widget w) 
{
    XExposeEvent xev;
    Region       region;

    xev.type       = Expose;
    xev.serial     = XLastKnownRequestProcessed(XtDisplay(w));
    xev.send_event = False;
    xev.display    = XtDisplay(w);
    xev.window     = XtWindowOfObject(w);
    xev.x          = 0;
    xev.y          = 0;
    xev.width      = w->core.width;
    xev.height     = w->core.height;
    xev.count      = 0;

    region = XCreateRegion();
    if (!region)
      return;

    XtAddExposureToRegion((XEvent*)&xev, region);    

    if (w->core.widget_class->core_class.expose)
        (*(w->core.widget_class->core_class.expose))(w, (XEvent *)&xev, region);

    XDestroyRegion(region);
}

/* Returns whether the widget passed in is a print shell or "print shell"-like
 * widget (e.g. print preview).
 * Note that this will return |True| for more classes than |XawPrintShell| in
 * the future (like for |XmPrintShell| etc.)
 */
Boolean 
XawIsPrintShell(Widget w)
{
    return XtIsSubclass(w, xawPrintShellWidgetClass);
}


static void
XawPrintNotify(Widget w, XtPointer client, XEvent *evp, Boolean *cont)
{
    XPPrintEvent                *e = (XPPrintEvent *)evp;
    XawPrintShellCallbackStruct  cbs;

    switch (e->detail) {
    case XPStartPageNotify:
        DEBUGOUT((__FILE__, w, "XPStartPageNotify\n"));

        /* Re do not have to call |XawPrintRedisplayWidget(w)| here since
         * Xprint triggers an expose event anyway
         */
        
        DEBUGOUT((__FILE__, w, "XpEndPage\n"));
        XpEndPage(XtDisplay(w));
        break;

    case XPEndPageNotify:
        DEBUGOUT((__FILE__, w, "XPEndPageNotify\n"));

        if (PS_LastPageInDoc(w) || PS_LastPageInJob(w)) {
            DEBUGOUT((__FILE__, w, "XpEndDoc\n"));
            XpEndDoc(XtDisplay(w));
        }
        else {
            /* Increment page numbers... */
            PS_CurrPageNumInDoc(w) += 1;
            PS_CurrPageNumInJob(w) += 1;

            /* ... do the page setup callback ... */
            cbs.reason           = XawCR_PAGE_SETUP;
            cbs.event            = evp;
            cbs.detail           = NULL;
            cbs.context          = XawPrintWidgetToContext(w);
            cbs.last_page_in_doc = False;
            cbs.last_page_in_job = False;

            if (PS_PageSetupCallback(w))
                XtCallCallbackList(w, PS_PageSetupCallback(w), &cbs);

            PS_LastPageInDoc(w) = cbs.last_page_in_doc;
            PS_LastPageInJob(w) = cbs.last_page_in_job;

            /* ... and start the new page */
            DEBUGOUT((__FILE__, w, "XpStartPage\n"));
            XpStartPage(XtDisplay(w), XtWindow(w));
        }
        break;

    case XPStartDocNotify:
        DEBUGOUT((__FILE__, w, "XPStartDocNotify\n"));

        cbs.reason           = XawCR_PAGE_SETUP;
        cbs.event            = evp;
        cbs.detail           = NULL;
        cbs.context          = XawPrintWidgetToContext(w);
        cbs.last_page_in_doc = False;
        cbs.last_page_in_job = False;

        if (PS_PageSetupCallback(w))
            XtCallCallbackList(w, PS_PageSetupCallback(w), &cbs);

        PS_LastPageInDoc(w) = cbs.last_page_in_doc;
        PS_LastPageInJob(w) = cbs.last_page_in_job;

        DEBUGOUT((__FILE__, w, "XpStartPage\n"));
        XpStartPage(XtDisplay(w), XtWindow(w));
        break;

    case XPEndDocNotify:
        DEBUGOUT((__FILE__, w, "XPEndDocNotify\n"));
        
        /* Start a new document (via XpStartDoc()) if we are not done with the job yet,
         * otherwise finish the job (via XpEndJob())
         */
        if (PS_LastPageInJob(w)) {
            DEBUGOUT((__FILE__, w, "XpEndJob\n"));
            XpEndJob(XtDisplay(w));
        }
        else {
            PS_CurrDocNumInJob(w) += 1;
            PS_CurrPageNumInDoc(w) = 1;

            cbs.reason           = XawCR_DOC_SETUP;
            cbs.event            = evp;
            cbs.detail           = NULL;
            cbs.context          = XawPrintWidgetToContext(w);
            cbs.last_page_in_doc = False;
            cbs.last_page_in_job = False;

            if (PS_DocSetupCallback(w))
                XtCallCallbackList(w, PS_DocSetupCallback(w), &cbs);

            PS_LastPageInDoc(w)  = cbs.last_page_in_doc;
            PS_LastPageInJob(w)  = cbs.last_page_in_job;

            DEBUGOUT((__FILE__, w, "XpStartDoc\n"));
            XpStartDoc(XtDisplay(w), XPDocNormal);
        }
        break;

    case XPStartJobNotify:
        DEBUGOUT((__FILE__, w, "XPStartJobNotify\n"));
        PS_LastPageInJob(w)    = False;
        PS_LastPageInDoc(w)    = False;
        PS_CurrDocNumInJob(w)  = 1;
        PS_CurrPageNumInDoc(w) = 1;
        PS_CurrPageNumInJob(w) = 1;

        cbs.reason           = XawCR_START_JOB;
        cbs.event            = evp;
        cbs.detail           = NULL;
        cbs.context          = XawPrintWidgetToContext(w);
        cbs.last_page_in_doc = False;
        cbs.last_page_in_job = False;

        if (PS_StartJobCallback(w))
            XtCallCallbackList(w, PS_StartJobCallback(w), &cbs);

        PS_LastPageInDoc(w)  = cbs.last_page_in_doc;
        PS_LastPageInJob(w)  = cbs.last_page_in_job;

        /* Start a document (which will trigger the first page in
         * |XPStartDocNotify| above) */
        if (PS_LastPageInDoc(w) || PS_LastPageInJob(w)) {
            DEBUGOUT((__FILE__, w, "XpEndJob\n"));
            XpEndJob(XtDisplay(w));
        }
        else
        {
            cbs.reason           = XawCR_DOC_SETUP;
            cbs.event            = evp;
            cbs.detail           = NULL;
            cbs.context          = XawPrintWidgetToContext(w);
            cbs.last_page_in_doc = False;
            cbs.last_page_in_job = False;

            if (PS_DocSetupCallback(w))
                XtCallCallbackList(w, PS_DocSetupCallback(w), &cbs);

            PS_LastPageInDoc(w)  = cbs.last_page_in_doc;
            PS_LastPageInJob(w)  = cbs.last_page_in_job;

            DEBUGOUT((__FILE__, w, "XpStartDoc\n"));
            XpStartDoc(XtDisplay(w), XPDocNormal);
        }
        break;

    case XPEndJobNotify:
        DEBUGOUT((__FILE__, w, "XPEndJobNotify\n"));
        cbs.reason           = XawCR_END_JOB;
        cbs.event            = evp;
        cbs.detail           = NULL;
        cbs.context          = None;
        cbs.last_page_in_doc = True;
        cbs.last_page_in_job = True;

        if (PS_EndJobCallback(w))
            XtCallCallbackList(w, PS_EndJobCallback(w), &cbs);
        break;

    default:
        DEBUGOUT((__FILE__, w, "XawPrintNotify(default)\n"));
        break;
    }
}

static void 
XawUpdateResources(Widget w, XPContext pcontext)
{
    XawPrintShellWidget print_shell = (XawPrintShellWidget)w;
    String              string_resolution;
    XRectangle          drawable_paper_area;

    string_resolution = XpGetOneAttribute(XtDisplay(w), pcontext, XPDocAttr, "default-printer-resolution");
    if (!string_resolution) {
      XtAppWarning(XtWidgetToApplicationContext(w),
                   "XawPrintShell: XawUpdateResources: Could not get 'default-printer-resolution' XPDocAttr\n");
    }
    print_shell->print.print_resolution = atol(string_resolution);
    XFree(string_resolution);

    if (print_shell->print.print_resolution == 0) {
      XtAppWarning(XtWidgetToApplicationContext(w),
                   "XawPrintShell: XawUpdateResources: Resolution '0' invalid\n");
    }

    /* Get the paper size... */
    XpGetPageDimensions(XtDisplay(w), pcontext,
                        &print_shell->print.page_width, &print_shell->print.page_height, 
                        &drawable_paper_area);
    /* ... and store it in the widget */
    print_shell->print.min_x = drawable_paper_area.x;
    print_shell->print.min_y = drawable_paper_area.y;
    print_shell->print.max_x = drawable_paper_area.x + drawable_paper_area.width;
    print_shell->print.max_y = drawable_paper_area.y + drawable_paper_area.height;
}

static void 
XawUpdateLayout(Widget w)
{
    XawPrintShellWidget  print_shell = (XawPrintShellWidget)w;

    switch( print_shell->print.layoutmode )
    {
      case XawPrintLAYOUTMODE_NONE:
          break;
      case XawPrintLAYOUTMODE_PAGESIZE:
          XtResizeWidget(w,
                         print_shell->print.page_width,
                         print_shell->print.page_height,
                         w->core.border_width);
          break;
      case XawPrintLAYOUTMODE_DRAWABLEAREA:
          XtConfigureWidget(w, 
                            print_shell->print.min_x,
                            print_shell->print.min_y, 
                            print_shell->print.max_x - print_shell->print.min_x, 
                            print_shell->print.max_y - print_shell->print.min_y,
                            w->core.border_width);
          break;
      default:
          XtAppWarning(XtWidgetToApplicationContext(w),
                       "XawPrintShell: XawUpdateResources: Invalid layout mode\n");
          break;
    }
}


/* Update widget attributes+properties when the we receive
 * "Xp attribute change"-events (e.g. paper (size/orientation/etc.)
 * changed etc.) */
static void 
XawAttributesNotify(Widget w,
                    XtPointer client,
                    XEvent *evp,
                    Boolean *cont)
{
    XawPrintShellWidget  print_shell = (XawPrintShellWidget)w;
    XPAttributeEvent    *xpevp       = (XPAttributeEvent *)evp;

    XawUpdateResources(w, xpevp->context);
    XawUpdateLayout(w);
}



