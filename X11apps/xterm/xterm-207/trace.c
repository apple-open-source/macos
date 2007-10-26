/* $XTermId: trace.c,v 1.63 2005/09/18 23:48:13 tom Exp $ */

/*
 * $XFree86: xc/programs/xterm/trace.c,v 3.23 2005/09/18 23:48:13 dickey Exp $
 */

/************************************************************

Copyright 1997-2004,2005 by Thomas E. Dickey

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the above listed
copyright holder(s) not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.

THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD
TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE
LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/

/*
 * debugging support via TRACE macro.
 */

#include <xterm.h>		/* for definition of GCC_UNUSED */
#include <data.h>
#include <trace.h>

#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>

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
char *trace_who = "parent";

void
Trace(char *fmt,...)
{
    static FILE *fp;
    static char *trace_out;
    va_list ap;

    if (fp != 0
	&& trace_who != trace_out) {
	fclose(fp);
	fp = 0;
    }
    trace_out = trace_who;

    if (!fp) {
	char name[BUFSIZ];
#if 0				/* usually I do not want unique names */
	int unique;
	for (unique = 0;; ++unique) {
	    if (unique)
		sprintf(name, "Trace-%s.out-%d", trace_who, unique);
	    else
		sprintf(name, "Trace-%s.out", trace_who);
	    if ((fp = fopen(name, "r")) == 0) {
		break;
	    }
	    fclose(fp);
	}
#else
	sprintf(name, "Trace-%s.out", trace_who);
#endif
	fp = fopen(name, "w");
	if (fp != 0) {
	    time_t now = time((time_t *) 0);
	    fprintf(fp, "%s\n", xtermVersion());
#ifdef HAVE_UNISTD_H
	    fprintf(fp, "process %d real (%u/%u) effective (%u/%u) -- %s",
		    getpid(),
		    (unsigned) getuid(), (unsigned) getgid(),
		    (unsigned) geteuid(), (unsigned) getegid(),
		    ctime(&now));
#else
	    fprintf(fp, "process %d -- %s",
		    getpid(),
		    ctime(&now));
#endif
	}
    }
    if (!fp)
	abort();

    va_start(ap, fmt);
    if (fmt != 0) {
	vfprintf(fp, fmt, ap);
	(void) fflush(fp);
    } else {
	(void) fclose(fp);
	(void) fflush(stdout);
	(void) fflush(stderr);
    }
    va_end(ap);
}

char *
visibleChars(PAIRED_CHARS(Char * buf, Char * buf2), unsigned len)
{
    static char *result;
    static unsigned used;
    unsigned limit = ((len + 1) * 8) + 1;
    char *dst;

    if (limit > used) {
	used = limit;
	result = XtRealloc(result, used);
    }
    dst = result;
    while (len--) {
	unsigned value = *buf++;
#if OPT_WIDE_CHARS
	if (buf2 != 0) {
	    value |= (*buf2 << 8);
	    buf2++;
	}
	if (value > 255)
	    sprintf(dst, "\\u+%04X", value);
	else
#endif
	if (E2A(value) < 32 || (E2A(value) >= 127 && E2A(value) < 160))
	    sprintf(dst, "\\%03o", value);
	else
	    sprintf(dst, "%c", CharOf(value));
	dst += strlen(dst);
    }
    return result;
}

char *
visibleIChar(IChar * buf, unsigned len)
{
    static char *result;
    static unsigned used;
    unsigned limit = ((len + 1) * 6) + 1;
    char *dst;

    if (limit > used) {
	used = limit;
	result = XtRealloc(result, used);
    }
    dst = result;
    while (len--) {
	unsigned value = *buf++;
#if OPT_WIDE_CHARS
	if (value > 255)
	    sprintf(dst, "\\u+%04X", value);
	else
#endif
	if (E2A(value) < 32 || (E2A(value) >= 127 && E2A(value) < 160))
	    sprintf(dst, "\\%03o", value);
	else
	    sprintf(dst, "%c", CharOf(value));
	dst += strlen(dst);
    }
    return result;
}

#define CASETYPE(name) case name: result = #name; break;

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
	CASETYPE(keyboardIsVT220);
    }
    return result;
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
    XSizeHints sizehints;
    long supp = 0;

    bzero(&sizehints, sizeof(sizehints));
    if (!XGetWMNormalHints(xw->screen.display, XtWindow(SHELL_OF(xw)),
			   &sizehints, &supp))
	bzero(&sizehints, sizeof(sizehints));
    TraceSizeHints(&sizehints);
}

/*
 * Some calls to XGetAtom() will fail, and we don't want to stop.  So we use
 * our own error-handler.
 */
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

    TRACE(("TraceTranslations for %s (widget %#lx)\n", name, (long) w));
    if (w) {
	XtVaGetValues(w,
		      XtNtranslations, &xlations,
		      XtNaccelerators, &xcelerat,
		      (XtPointer) 0);
	TRACE(("... xlations %#08lx\n", (long) xlations));
	TRACE(("... xcelerat %#08lx\n", (long) xcelerat));
	result = _XtPrintXlations(w, xlations, xcelerat, True);
	TRACE(("%s\n", result != 0 ? result : "(null)"));
	if (result)
	    XFree(result);
    } else {
	TRACE(("none (widget is null)\n"));
    }
    XSetErrorHandler(save);
}

#define XRES_S(name) Trace(#name " = %s\n", NonNull(resp->name))
#define XRES_B(name) Trace(#name " = %s\n", BtoS(resp->name))
#define XRES_I(name) Trace(#name " = %d\n", resp->name)

void
TraceXtermResources(void)
{
    XTERM_RESOURCE *resp = &resource;

    Trace("XTERM_RESOURCE settings:\n");
    XRES_S(xterm_name);
    XRES_S(icon_geometry);
    XRES_S(title);
    XRES_S(icon_name);
    XRES_S(term_name);
    XRES_S(tty_modes);
    XRES_B(hold_screen);
    XRES_B(utmpInhibit);
    XRES_B(utmpDisplayId);
    XRES_B(messages);
    XRES_B(sunFunctionKeys);
#if OPT_SUNPC_KBD
    XRES_B(sunKeyboard);
#endif
#if OPT_HP_FUNC_KEYS
    XRES_B(hpFunctionKeys);
#endif
#if OPT_SCO_FUNC_KEYS
    XRES_B(scoFunctionKeys);
#endif
#if OPT_INITIAL_ERASE
    XRES_B(ptyInitialErase);
    XRES_B(backarrow_is_erase);
#endif
    XRES_B(wait_for_map);
    XRES_B(useInsertMode);
#if OPT_ZICONBEEP
    XRES_I(zIconBeep);
#endif
#if OPT_PTY_HANDSHAKE
    XRES_B(ptyHandshake);
#endif
#if OPT_SAME_NAME
    XRES_B(sameName);
#endif
#if OPT_SESSION_MGT
    XRES_B(sessionMgt);
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
parse_option(char *dst, char *src, int first)
{
    char *s;

    if (!strncmp(src, "-/+", 3)) {
	dst[0] = first;
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
standard_option(char *opt)
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
	default:
	    break;
	}
    }
}
