/* $XTermId: trace.h,v 1.35 2005/09/18 23:48:13 tom Exp $ */

/*
 * $XFree86: xc/programs/xterm/trace.h,v 3.17 2005/09/18 23:48:13 dickey Exp $
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
 * Common/useful definitions for XTERM application
 */
#ifndef	included_trace_h
#define	included_trace_h

#include <xterm.h>

#if OPT_TRACE

extern	void	Trace ( char *, ... )
#ifdef GCC_PRINTF
	__attribute__ ((format(printf,1,2)))
#endif
	;
#define TRACE(p) Trace p

#if OPT_TRACE > 1
#define TRACE2(p) Trace p
#endif

extern	char *	visibleChars (PAIRED_CHARS(Char *buf, Char *buf2), unsigned len);
extern	char *	visibleIChar (IChar *, unsigned);

extern	void	TraceArgv(const char *tag, char **argv);
#define	TRACE_ARGV(tag,argv) TraceArgv(tag,argv)

extern	char	*trace_who;
#define TRACE_CHILD int tracing_child = (trace_who = "child") != 0;

extern	void	TraceSizeHints(XSizeHints *);
#define	TRACE_HINTS(hints) TraceSizeHints(hints)

extern	void	TraceOptions(OptionHelp *options, XrmOptionDescRec *resources, Cardinal count);
#define	TRACE_OPTS(opts,ress,lens) TraceOptions(opts,ress,lens)

extern	void	TraceTranslations(const char *, Widget);
#define	TRACE_TRANS(name,w) TraceTranslations(name,w)

extern	void	TraceWMSizeHints(XtermWidget);
#define	TRACE_WM_HINTS(w) TraceWMSizeHints(w)

extern	void	TraceXtermResources(void);
#define	TRACE_XRES() TraceXtermResources()

#endif

#endif	/* included_trace_h */
