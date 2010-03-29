/* $XTermId: trace.h,v 1.48 2009/10/12 00:08:18 tom Exp $ */

/************************************************************

Copyright 1997-2008,2009 by Thomas E. Dickey

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

extern	void	Trace ( const char *, ... )
#ifdef GCC_PRINTF
	__attribute__ ((format(printf,1,2)))
#endif
	;
#undef  TRACE
#define TRACE(p) Trace p

#if OPT_TRACE > 1
#define TRACE2(p) Trace p
#endif

extern	char *	visibleChars (Char * /* buf */, unsigned /* len */);
extern	char *	visibleIChar (IChar *, unsigned);
extern	char *	visibleIChars (IChar * /* buf */, unsigned /* len */);
extern	const char * visibleEventType (int);
extern	const char * visibleSelectionTarget(Display * /* d */, Atom /* a */);
extern	const char * visibleXError (int /* code */);
extern  const char * visibleChrsetName(unsigned /* chrset */);

extern	void	TraceArgv(const char * /* tag */, char ** /* argv */);
#undef  TRACE_ARGV
#define	TRACE_ARGV(tag,argv) TraceArgv(tag,argv)

extern	const	char *trace_who;
#undef  TRACE_CHILD
#define TRACE_CHILD int tracing_child = (trace_who = "child") != 0; (void) tracing_child;

extern	void	TraceSizeHints(XSizeHints *);
#undef  TRACE_HINTS
#define	TRACE_HINTS(hints) TraceSizeHints(hints)

extern	void	TraceIds(const char * /* fname */, int  /* lnum */);
#undef  TRACE_IDS
#define	TRACE_IDS TraceIds(__FILE__, __LINE__)

extern	void	TraceOptions(OptionHelp * /* options */, XrmOptionDescRec * /* resources */, Cardinal  /* count */);
#undef  TRACE_OPTS
#define	TRACE_OPTS(opts,ress,lens) TraceOptions(opts,ress,lens)

extern	void	TraceTranslations(const char *, Widget);
#undef  TRACE_TRANS
#define	TRACE_TRANS(name,w) TraceTranslations(name,w)

extern	void	TraceWMSizeHints(XtermWidget);
#undef  TRACE_WM_HINTS
#define	TRACE_WM_HINTS(w) TraceWMSizeHints(w)

extern	void	TraceXtermResources(void);
#undef  TRACE_XRES
#define	TRACE_XRES() TraceXtermResources()

extern	int	TraceResizeRequest(const char * /* fn */, int  /* ln */, Widget  /* w */, Dimension  /* reqwide */, Dimension  /* reqhigh */, Dimension * /* gotwide */, Dimension * /* gothigh */);
#undef  REQ_RESIZE
#define REQ_RESIZE(w, reqwide, reqhigh, gotwide, gothigh) \
	TraceResizeRequest(__FILE__, __LINE__, w, reqwide, reqhigh, gotwide, gothigh)

#else

#define REQ_RESIZE(w, reqwide, reqhigh, gotwide, gothigh) \
	XtMakeResizeRequest((Widget) (w), \
			    (Dimension) (reqwide), (Dimension) (reqhigh), \
			    (gotwide), (gothigh))

#endif

#endif	/* included_trace_h */
