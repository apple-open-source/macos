/* $XTermId: trace.c,v 1.121 2011/02/17 00:34:47 tom Exp $ */

/*
 * Copyright 1997-2010,2011 by Thomas E. Dickey
 * 
 *                         All Rights Reserved
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 * 
 */

/*
 * debugging support via TRACE macro.
 */

#include <xterm.h>		/* for definition of GCC_UNUSED */

#if OPT_TRACE

#include <data.h>
#include <trace.h>

#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h>

#ifdef HAVE_X11_TRANSLATEI_H
#include <X11/TranslateI.h>
#else
#ifdef __cplusplus
extern "C" {
#endif

    extern String _XtPrintXlations(Widget w,
				   XtTranslations xlations,
				   Widget accelWidget,
				   _XtBoolean includeRHS);
#ifdef __cplusplus
}
#endif
#endif
const char *trace_who = "parent";

static FILE *trace_fp;

void
Trace(const char *fmt,...)
{
    static const char *trace_out;
    va_list ap;

    if (trace_fp != 0
	&& trace_who != trace_out) {
	fclose(trace_fp);
	trace_fp = 0;
    }
    trace_out = trace_who;

    if (!trace_fp) {
	char name[BUFSIZ];
#if 0				/* usually I do not want unique names */
	int unique;
	for (unique = 0;; ++unique) {
	    if (unique)
		sprintf(name, "Trace-%s.out-%d", trace_who, unique);
	    else
		sprintf(name, "Trace-%s.out", trace_who);
	    if ((trace_fp = fopen(name, "r")) == 0) {
		break;
	    }
	    fclose(trace_fp);
	}
#else
	sprintf(name, "Trace-%s.out", trace_who);
#endif
	trace_fp = fopen(name, "w");
	if (trace_fp != 0) {
	    fprintf(trace_fp, "%s\n", xtermVersion());
	    TraceIds(NULL, 0);
	}
    }
    if (!trace_fp)
	abort();

    va_start(ap, fmt);
    vfprintf(trace_fp, fmt, ap);
    (void) fflush(trace_fp);
    va_end(ap);
}

void
TraceClose(void)
{
    if (trace_fp != 0) {
	(void) fclose(trace_fp);
	(void) fflush(stdout);
	(void) fflush(stderr);
	(void) visibleChars(NULL, 0);
	(void) visibleIChars(NULL, 0);
	(void) visibleIChar(NULL, 0);
	trace_fp = 0;
    }
}

void
TraceIds(const char *fname, int lnum)
{
    Trace("process %d ", (int) getpid());
#ifdef HAVE_UNISTD_H
    Trace("real (%u/%u) effective (%u/%u)",
	  (unsigned) getuid(), (unsigned) getgid(),
	  (unsigned) geteuid(), (unsigned) getegid());
#endif
    if (fname != 0) {
	Trace(" (%s@%d)\n", fname, lnum);
    } else {
	time_t now = time((time_t *) 0);
	Trace("-- %s", ctime(&now));
    }
}

static void
formatAscii(char *dst, unsigned value)
{
    switch (value) {
    case '\\':
	sprintf(dst, "\\\\");
	break;
    case '\b':
	sprintf(dst, "\\b");
	break;
    case '\n':
	sprintf(dst, "\\n");
	break;
    case '\r':
	sprintf(dst, "\\r");
	break;
    case '\t':
	sprintf(dst, "\\t");
	break;
    default:
	if (E2A(value) < 32 || (E2A(value) >= 127 && E2A(value) < 160))
	    sprintf(dst, "\\%03o", value);
	else
	    sprintf(dst, "%c", CharOf(value));
	break;
    }
}

#if OPT_DEC_CHRSET

const char *
visibleChrsetName(unsigned chrset)
{
    const char *result = "?";
    switch (chrset) {
    case CSET_SWL:
	result = "CSET_SWL";
	break;
    case CSET_DHL_TOP:
	result = "CSET_DHL_TOP";
	break;
    case CSET_DHL_BOT:
	result = "CSET_DHL_BOT";
	break;
    case CSET_DWL:
	result = "CSET_DWL";
	break;
    }
    return result;
}
#endif

char *
visibleChars(const Char * buf, unsigned len)
{
    static char *result;
    static unsigned used;

    if (buf != 0) {
	unsigned limit = ((len + 1) * 8) + 1;
	char *dst;

	if (limit > used) {
	    used = limit;
	    result = XtRealloc(result, used);
	}
	if (result != 0) {
	    dst = result;
	    *dst = '\0';
	    while (len--) {
		unsigned value = *buf++;
		formatAscii(dst, value);
		dst += strlen(dst);
	    }
	}
    } else if (result != 0) {
	free(result);
	result = 0;
	used = 0;
    }
    return result;
}

char *
visibleIChars(IChar * buf, unsigned len)
{
    static char *result;
    static unsigned used;

    if (buf != 0) {
	unsigned limit = ((len + 1) * 8) + 1;
	char *dst;

	if (limit > used) {
	    used = limit;
	    result = XtRealloc(result, used);
	}
	if (result != 0) {
	    dst = result;
	    *dst = '\0';
	    while (len--) {
		unsigned value = *buf++;
#if OPT_WIDE_CHARS
		if (value > 255)
		    sprintf(dst, "\\u+%04X", value);
		else
#endif
		    formatAscii(dst, value);
		dst += strlen(dst);
	    }
	}
    } else if (result != 0) {
	free(result);
	result = 0;
	used = 0;
    }
    return result;
}

char *
visibleIChar(IChar * buf, unsigned len)
{
    static char *result;
    static unsigned used;

    if (buf != 0) {
	unsigned limit = ((len + 1) * 8) + 1;
	char *dst;

	if (limit > used) {
	    used = limit;
	    result = XtRealloc(result, used);
	}
	if (result != 0) {
	    dst = result;
	    while (len--) {
		unsigned value = *buf++;
#if OPT_WIDE_CHARS
		if (value > 255)
		    sprintf(dst, "\\u+%04X", value);
		else
#endif
		    formatAscii(dst, value);
		dst += strlen(dst);
	    }
	}
    } else if (result != 0) {
	free(result);
	result = 0;
	used = 0;
    }
    return result;
}

#define CASETYPE(name) case name: result = #name; break

const char *
visibleKeyboardType(xtermKeyboardType type)
{
    const char *result = "?";
    switch (type) {
	CASETYPE(keyboardIsLegacy);	/* bogus vt220 codes for F1-F4, etc. */
	CASETYPE(keyboardIsDefault);
	CASETYPE(keyboardIsHP);
	CASETYPE(keyboardIsSCO);
	CASETYPE(keyboardIsSun);
	CASETYPE(keyboardIsTermcap);
	CASETYPE(keyboardIsVT220);
    }
    return result;
}

const char *
visibleEventType(int type)
{
    const char *result = "?";
    switch (type) {
	CASETYPE(KeyPress);
	CASETYPE(KeyRelease);
	CASETYPE(ButtonPress);
	CASETYPE(ButtonRelease);
	CASETYPE(MotionNotify);
	CASETYPE(EnterNotify);
	CASETYPE(LeaveNotify);
	CASETYPE(FocusIn);
	CASETYPE(FocusOut);
	CASETYPE(KeymapNotify);
	CASETYPE(Expose);
	CASETYPE(GraphicsExpose);
	CASETYPE(NoExpose);
	CASETYPE(VisibilityNotify);
	CASETYPE(CreateNotify);
	CASETYPE(DestroyNotify);
	CASETYPE(UnmapNotify);
	CASETYPE(MapNotify);
	CASETYPE(MapRequest);
	CASETYPE(ReparentNotify);
	CASETYPE(ConfigureNotify);
	CASETYPE(ConfigureRequest);
	CASETYPE(GravityNotify);
	CASETYPE(ResizeRequest);
	CASETYPE(CirculateNotify);
	CASETYPE(CirculateRequest);
	CASETYPE(PropertyNotify);
	CASETYPE(SelectionClear);
	CASETYPE(SelectionRequest);
	CASETYPE(SelectionNotify);
	CASETYPE(ColormapNotify);
	CASETYPE(ClientMessage);
	CASETYPE(MappingNotify);
    }
    return result;
}

const char *
visibleNotifyDetail(int code)
{
    const char *result = "?";
    switch (code) {
	CASETYPE(NotifyAncestor);
	CASETYPE(NotifyVirtual);
	CASETYPE(NotifyInferior);
	CASETYPE(NotifyNonlinear);
	CASETYPE(NotifyNonlinearVirtual);
	CASETYPE(NotifyPointer);
	CASETYPE(NotifyPointerRoot);
	CASETYPE(NotifyDetailNone);
    }
    return result;
}

const char *
visibleSelectionTarget(Display * d, Atom a)
{
    const char *result = "?";

    if (a == XA_STRING) {
	result = "XA_STRING";
    } else if (a == XA_TEXT(d)) {
	result = "XA_TEXT()";
    } else if (a == XA_COMPOUND_TEXT(d)) {
	result = "XA_COMPOUND_TEXT()";
    } else if (a == XA_UTF8_STRING(d)) {
	result = "XA_UTF8_STRING()";
    } else if (a == XA_TARGETS(d)) {
	result = "XA_TARGETS()";
    }

    return result;
}

const char *
visibleXError(int code)
{
    static char temp[80];
    const char *result = "?";
    switch (code) {
	CASETYPE(Success);
	CASETYPE(BadRequest);
	CASETYPE(BadValue);
	CASETYPE(BadWindow);
	CASETYPE(BadPixmap);
	CASETYPE(BadAtom);
	CASETYPE(BadCursor);
	CASETYPE(BadFont);
	CASETYPE(BadMatch);
	CASETYPE(BadDrawable);
	CASETYPE(BadAccess);
	CASETYPE(BadAlloc);
	CASETYPE(BadColor);
	CASETYPE(BadGC);
	CASETYPE(BadIDChoice);
	CASETYPE(BadName);
	CASETYPE(BadLength);
	CASETYPE(BadImplementation);
    default:
	sprintf(temp, "%d", code);
	result = temp;
	break;
    }
    return result;
}

#if OPT_TRACE_FLAGS
#define isScrnFlag(flag) ((flag) == LINEWRAPPED)

static char *
ScrnText(LineData * ld)
{
    return visibleIChars(ld->charData, ld->lineSize);
}

#define SHOW_BAD_LINE(name, ld) \
	Trace("OOPS " #name " bad row\n")

#define SHOW_SCRN_FLAG(name,code) \
	Trace(#name " %s:%s\n", \
	      code ? "*" : "", \
	      ScrnText(ld))

void
LineClrFlag(LineData * ld, int flag)
{
    if (ld == 0) {
	SHOW_BAD_LINE(LineClrFlag, ld);
	assert(0);
    } else if (isScrnFlag(flag)) {
	SHOW_SCRN_FLAG(LineClrFlag, 0);
    }

    LineFlags(ld) &= ~flag;
}

void
LineSetFlag(LineData * ld, int flag)
{
    if (ld == 0) {
	SHOW_BAD_LINE(LineSetFlag, ld);
	assert(0);
    } else if (isScrnFlag(flag)) {
	SHOW_SCRN_FLAG(LineSetFlag, 1);
    }

    LineFlags(ld) |= flag;
}

int
LineTstFlag(LineData ld, int flag)
{
    int code = 0;
    if (ld == 0) {
	SHOW_BAD_LINE(LineTstFlag, ld);
    } else {
	code = LineFlags(ld);

	if (isScrnFlag(flag)) {
	    SHOW_SCRN_FLAG(LineTstFlag, code);
	}
    }
    return code;
}
#endif /* OPT_TRACE_FLAGS */

void
TraceFocus(Widget w, XEvent * ev)
{
    TRACE(("trace_focus event type %d:%s\n",
	   ev->type, visibleEventType(ev->type)));
    switch (ev->type) {
    case FocusIn:
    case FocusOut:
	{
	    XFocusChangeEvent *event = (XFocusChangeEvent *) ev;
	    TRACE(("\tdetail: %s\n", visibleNotifyDetail(event->detail)));
	    TRACE(("\tmode:   %d\n", event->mode));
	    TRACE(("\twindow: %#lx\n", event->window));
	}
	break;
    case EnterNotify:
    case LeaveNotify:
	{
	    XCrossingEvent *event = (XCrossingEvent *) ev;
	    TRACE(("\tdetail:    %s\n", visibleNotifyDetail(event->detail)));
	    TRACE(("\tmode:      %d\n", event->mode));
	    TRACE(("\twindow:    %#lx\n", event->window));
	    TRACE(("\troot:      %#lx\n", event->root));
	    TRACE(("\tsubwindow: %#lx\n", event->subwindow));
	}
	break;
    }
    while (w != 0) {
	TRACE(("w %p -> %#lx\n", (void *) w, XtWindow(w)));
	w = XtParent(w);
    }
}

void
TraceSizeHints(XSizeHints * hints)
{
    TRACE(("size hints:\n"));
    if (hints->flags & (USPosition | PPosition))
	TRACE(("   position   %d,%d%s%s\n", hints->y, hints->x,
	       hints->flags & USPosition ? " user" : "",
	       hints->flags & PPosition ? " prog" : ""));
    if (hints->flags & (USSize | PSize))
	TRACE(("   size       %d,%d%s%s\n", hints->height, hints->width,
	       hints->flags & USSize ? " user" : "",
	       hints->flags & PSize ? " prog" : ""));
    if (hints->flags & PMinSize)
	TRACE(("   min        %d,%d\n", hints->min_height, hints->min_width));
    if (hints->flags & PMaxSize)
	TRACE(("   max        %d,%d\n", hints->max_height, hints->max_width));
    if (hints->flags & PResizeInc)
	TRACE(("   inc        %d,%d\n", hints->height_inc, hints->width_inc));
    else
	TRACE(("   inc        NONE!\n"));
    if (hints->flags & PAspect)
	TRACE(("   min aspect %d/%d\n", hints->min_aspect.y, hints->min_aspect.y));
    if (hints->flags & PAspect)
	TRACE(("   max aspect %d/%d\n", hints->max_aspect.y, hints->max_aspect.y));
    if (hints->flags & PBaseSize)
	TRACE(("   base       %d,%d\n", hints->base_height, hints->base_width));
    if (hints->flags & PWinGravity)
	TRACE(("   gravity    %d\n", hints->win_gravity));
}

void
TraceWMSizeHints(XtermWidget xw)
{
    XSizeHints sizehints = xw->hints;

    getXtermSizeHints(xw);
    TraceSizeHints(&xw->hints);
    xw->hints = sizehints;
}

/*
 * Some calls to XGetAtom() will fail, and we don't want to stop.  So we use
 * our own error-handler.
 */
/* ARGSUSED */
static int
no_error(Display * dpy GCC_UNUSED, XErrorEvent * event GCC_UNUSED)
{
    return 1;
}

void
TraceTranslations(const char *name, Widget w)
{
    String result;
    XErrorHandler save = XSetErrorHandler(no_error);
    XtTranslations xlations;
    Widget xcelerat;

    TRACE(("TraceTranslations for %s (widget %#lx) {{\n", name, (long) w));
    if (w) {
	XtVaGetValues(w,
		      XtNtranslations, &xlations,
		      XtNaccelerators, &xcelerat,
		      (XtPointer) 0);
	TRACE(("... xlations %#08lx\n", (long) xlations));
	TRACE(("... xcelerat %#08lx\n", (long) xcelerat));
	result = _XtPrintXlations(w, xlations, xcelerat, True);
	TRACE(("%s\n", NonNull(result)));
	if (result)
	    XFree((char *) result);
    } else {
	TRACE(("none (widget is null)\n"));
    }
    TRACE(("}}\n"));
    XSetErrorHandler(save);
}

int
TraceResizeRequest(const char *fn, int ln, Widget w,
		   Dimension reqwide,
		   Dimension reqhigh,
		   Dimension * gotwide,
		   Dimension * gothigh)
{
    int rc;

    TRACE(("%s@%d ResizeRequest %dx%d\n", fn, ln, reqhigh, reqwide));
    rc = XtMakeResizeRequest((Widget) w, reqwide, reqhigh, gotwide, gothigh);
    TRACE(("... ResizeRequest -> "));
    if (gothigh && gotwide)
	TRACE(("%dx%d ", *gothigh, *gotwide));
    TRACE(("(%d)\n", rc));
    return rc;
}

#define XRES_S(name) Trace(#name " = %s\n", NonNull(resp->name))
#define XRES_B(name) Trace(#name " = %s\n", BtoS(resp->name))
#define XRES_I(name) Trace(#name " = %d\n", resp->name)

void
TraceXtermResources(void)
{
    XTERM_RESOURCE *resp = &resource;

    Trace("XTERM_RESOURCE settings:\n");
    XRES_S(icon_geometry);
    XRES_S(title);
    XRES_S(icon_name);
    XRES_S(term_name);
    XRES_S(tty_modes);
    XRES_B(hold_screen);
    XRES_B(utmpInhibit);
    XRES_B(utmpDisplayId);
    XRES_B(messages);
#if OPT_SUNPC_KBD
    XRES_B(sunKeyboard);
#endif
#if OPT_HP_FUNC_KEYS
    XRES_B(hpFunctionKeys);
#endif
#if OPT_SCO_FUNC_KEYS
    XRES_B(scoFunctionKeys);
#endif
#if OPT_SUN_FUNC_KEYS
    XRES_B(sunFunctionKeys);
#endif
#if OPT_INITIAL_ERASE
    XRES_B(ptyInitialErase);
    XRES_B(backarrow_is_erase);
#endif
    XRES_B(useInsertMode);
#if OPT_ZICONBEEP
    XRES_I(zIconBeep);
#endif
#if OPT_PTY_HANDSHAKE
    XRES_B(wait_for_map);
    XRES_B(ptyHandshake);
    XRES_B(ptySttySize);
#endif
#if OPT_SAME_NAME
    XRES_B(sameName);
#endif
#if OPT_SESSION_MGT
    XRES_B(sessionMgt);
#endif
#if OPT_TOOLBAR
    XRES_B(toolBar);
#endif
#if OPT_MAXIMIZE
    XRES_B(maximized);
    XRES_S(fullscreen_s);
#endif
}

void
TraceArgv(const char *tag, char **argv)
{
    int n = 0;

    TRACE(("%s:\n", tag));
    while (*argv != 0) {
	TRACE(("  %d:%s\n", n++, *argv++));
    }
}

static char *
parse_option(char *dst, String src, int first)
{
    char *s;

    if (!strncmp(src, "-/+", (size_t) 3)) {
	dst[0] = (char) first;
	strcpy(dst + 1, src + 3);
    } else {
	strcpy(dst, src);
    }
    for (s = dst; *s != '\0'; s++) {
	if (*s == '#' || *s == '%' || *s == 'S') {
	    s[1] = '\0';
	} else if (*s == ' ') {
	    *s = '\0';
	    break;
	}
    }
    return dst;
}

static Bool
same_option(OptionHelp * opt, XrmOptionDescRec * res)
{
    char temp[BUFSIZ];
    return !strcmp(parse_option(temp, opt->opt, res->option[0]), res->option);
}

static Bool
standard_option(String opt)
{
    static const char *table[] =
    {
	"+rv",
	"+synchronous",
	"-background",
	"-bd",
	"-bg",
	"-bordercolor",
	"-borderwidth",
	"-bw",
	"-display",
	"-fg",
	"-fn",
	"-font",
	"-foreground",
	"-geometry",
	"-iconic",
	"-name",
	"-reverse",
	"-rv",
	"-selectionTimeout",
	"-synchronous",
	"-title",
	"-xnllanguage",
	"-xrm",
	"-xtsessionID",
    };
    Cardinal n;
    char temp[BUFSIZ];

    opt = parse_option(temp, opt, '-');
    for (n = 0; n < XtNumber(table); n++) {
	if (!strcmp(opt, table[n]))
	    return True;
    }
    return False;
}

/*
 * Analyse the options/help messages for inconsistencies.
 */
void
TraceOptions(OptionHelp * options, XrmOptionDescRec * resources, Cardinal res_count)
{
    OptionHelp *opt_array = sortedOpts(options, resources, res_count);
    size_t j, k;
    XrmOptionDescRec *res_array = sortedOptDescs(resources, res_count);
    Bool first, found;

    TRACE(("Checking options-tables for inconsistencies:\n"));

#if 0
    TRACE(("Options listed in help-message:\n"));
    for (j = 0; options[j].opt != 0; j++)
	TRACE(("%5d %-28s %s\n", j, opt_array[j].opt, opt_array[j].desc));
    TRACE(("Options listed in resource-table:\n"));
    for (j = 0; j < res_count; j++)
	TRACE(("%5d %-28s %s\n", j, res_array[j].option, res_array[j].specifier));
#endif

    /* list all options[] not found in resources[] */
    for (j = 0, first = True; options[j].opt != 0; j++) {
	found = False;
	for (k = 0; k < res_count; k++) {
	    if (same_option(&opt_array[j], &res_array[k])) {
		found = True;
		break;
	    }
	}
	if (!found) {
	    if (first) {
		TRACE(("Options listed in help, not found in resource list:\n"));
		first = False;
	    }
	    TRACE(("  %-28s%s\n", opt_array[j].opt,
		   standard_option(opt_array[j].opt) ? " (standard)" : ""));
	}
    }

    /* list all resources[] not found in options[] */
    for (j = 0, first = True; j < res_count; j++) {
	found = False;
	for (k = 0; options[k].opt != 0; k++) {
	    if (same_option(&opt_array[k], &res_array[j])) {
		found = True;
		break;
	    }
	}
	if (!found) {
	    if (first) {
		TRACE(("Resource list items not found in options-help:\n"));
		first = False;
	    }
	    TRACE(("  %s\n", res_array[j].option));
	}
    }

    TRACE(("Resource list items that will be ignored by XtOpenApplication:\n"));
    for (j = 0; j < res_count; j++) {
	switch (res_array[j].argKind) {
	case XrmoptionSkipArg:
	    TRACE(("  %-28s {param}\n", res_array[j].option));
	    break;
	case XrmoptionSkipNArgs:
	    TRACE(("  %-28s {%ld params}\n", res_array[j].option, (long)
		   res_array[j].value));
	    break;
	case XrmoptionSkipLine:
	    TRACE(("  %-28s {remainder of line}\n", res_array[j].option));
	    break;
	case XrmoptionIsArg:
	case XrmoptionNoArg:
	case XrmoptionResArg:
	case XrmoptionSepArg:
	case XrmoptionStickyArg:
	default:
	    break;
	}
    }
}
#else
extern void empty_trace(void);
void
empty_trace(void)
{
}
#endif
