/*
 * $XFree86: xc/programs/xterm/trace.c,v 3.16 2002/10/05 17:57:13 dickey Exp $
 */

/************************************************************

Copyright 1997-2001 by Thomas E. Dickey

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
extern String _XtPrintXlations(Widget w,
			       XtTranslations xlations,
			       Widget accelWidget,
			       _XtBoolean includeRHS);
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
	sprintf(name, "Trace-%s.out", trace_who);
	fp = fopen(name, "w");
	if (fp != 0) {
	    time_t now = time((time_t *) 0);
#ifdef HAVE_UNISTD_H
	    fprintf(fp, "process %d real (%d/%d) effective (%d/%d) -- %s",
		    getpid(),
		    getuid(), getgid(),
		    geteuid(), getegid(),
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
	    sprintf(dst, "%c", value);
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
	    sprintf(dst, "%c", value);
	dst += strlen(dst);
    }
    return result;
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
    } else {
	TRACE(("none (widget is null)\n"));
    }
    XSetErrorHandler(save);
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
parse_option(char *dst, char *src, char first)
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

static Boolean
same_option(OptionHelp * opt, XrmOptionDescRec * res)
{
    char temp[BUFSIZ];
    return !strcmp(parse_option(temp, opt->opt, res->option[0]), res->option);
}

static Boolean
standard_option(char *opt)
{
    static char *table[] =
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
    Boolean first, found;

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
