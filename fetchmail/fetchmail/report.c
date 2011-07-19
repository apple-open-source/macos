/** \file report.c report function for noninteractive utilities
 *
 * For license terms, see the file COPYING in this directory.
 *
 * This code is distantly descended from the error.c module written by
 * David MacKenzie <djm@gnu.ai.mit.edu>.  It was redesigned and
 * rewritten by Dave Bodenstab, then redesigned again by ESR, then
 * bludgeoned into submission for SunOS 4.1.3 by Chris Cheyney
 * <cheyney@netcom.com>.  It works even when the return from
 * vprintf(3) is unreliable.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <errno.h>
#include <string.h>
#if defined(HAVE_SYSLOG)
#include <syslog.h>
#endif
#include "i18n.h"
#include "fetchmail.h"

#if defined(HAVE_VPRINTF) || defined(HAVE_DOPRNT) || defined(_LIBC) || defined(HAVE_STDARG_H)
# if HAVE_STDARG_H
#  include <stdarg.h>
#  define VA_START(args, lastarg) va_start(args, lastarg)
# else
#  include <varargs.h>
#  define VA_START(args, lastarg) va_start(args)
# endif
#else
# define va_alist a1, a2, a3, a4, a5, a6, a7, a8
# define va_dcl char *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8;
#endif

#define MALLOC(n)	xmalloc(n)	
#define REALLOC(n,s)	xrealloc(n,s)	

/* Used by report_build() and report_complete() to accumulate partial messages.
 */
static unsigned int partial_message_size = 0;
static unsigned int partial_message_size_used = 0;
static char *partial_message;
static int partial_suppress_tag = 0;

static unsigned unbuffered;
static unsigned int use_syslog;

#ifdef _LIBC
/* In the GNU C library, there is a predefined variable for this.  */

# define program_name program_invocation_name
# include <errno.h>

#else

# if !HAVE_STRERROR && !defined(strerror)
char *strerror (int errnum)
{
    extern char *sys_errlist[];
    extern int sys_nerr;

    if (errnum > 0 && errnum <= sys_nerr)
	return sys_errlist[errnum];
    return GT_("Unknown system error");
}
# endif	/* HAVE_STRERROR */
#endif	/* _LIBC */

/* Print the program name and error message MESSAGE, which is a printf-style
   format string with optional args. */
/* VARARGS */
void
#ifdef HAVE_STDARG_H
report (FILE *errfp, const char *message, ...)
#else
report (FILE *errfp, message, va_alist)
     const char *message;
     va_dcl
#endif
{
#ifdef VA_START
    va_list args;
#endif

    /* If a partially built message exists, print it now so it's not lost.  */
    if (partial_message_size_used != 0)
    {
	partial_message_size_used = 0;
	report (errfp, GT_("%s (log message incomplete)\n"), partial_message);
    }

#if defined(HAVE_SYSLOG)
    if (use_syslog)
    {
	int priority;

#ifdef VA_START
	VA_START (args, message);
#endif
	priority = (errfp == stderr) ? LOG_ERR : LOG_INFO;

#ifdef HAVE_VSYSLOG
	vsyslog (priority, message, args);
#else
	{
	    char *a1 = va_arg(args, char *);
	    char *a2 = va_arg(args, char *);
	    char *a3 = va_arg(args, char *);
	    char *a4 = va_arg(args, char *);
	    char *a5 = va_arg(args, char *);
	    char *a6 = va_arg(args, char *);
	    char *a7 = va_arg(args, char *);
	    char *a8 = va_arg(args, char *);
	    syslog (priority, message, a1, a2, a3, a4, a5, a6, a7, a8);
	}
#endif

#ifdef VA_START
	va_end(args);
#endif
    }
    else /* i. e. not using syslog */
#endif
    {
	if ( *message == '\n' )
	{
	    fputc( '\n', errfp );
	    ++message;
	}
	if (!partial_suppress_tag)
		fprintf (errfp, "%s: ", program_name);
	partial_suppress_tag = 0;

#ifdef VA_START
	VA_START (args, message);
# if defined(HAVE_VPRINTF) || defined(_LIBC)
	vfprintf (errfp, message, args);
# else
	_doprnt (message, args, errfp);
# endif
	va_end (args);
#else
	fprintf (errfp, message, a1, a2, a3, a4, a5, a6, a7, a8);
#endif
	fflush (errfp);
    }
}

/**
 * Configure the report module. The output is set according to
 * \a mode.
 */
void report_init(int mode /** 0: regular output, 1: unbuffered output, -1: syslog */)
{
    switch(mode)
    {
    case 0:			/* errfp, buffered */
    default:
	unbuffered = FALSE;
	use_syslog = FALSE;
	break;

    case 1:			/* errfp, unbuffered */
	unbuffered = TRUE;
	use_syslog = FALSE;
	break;

#ifdef HAVE_SYSLOG
    case -1:			/* syslogd */
	unbuffered = FALSE;
	use_syslog = TRUE;
	break;
#endif /* HAVE_SYSLOG */
    }
}

/* Build an report message by appending MESSAGE, which is a printf-style
   format string with optional args, to the existing report message (which may
   be empty.)  The completed report message is finally printed (and reset to
   empty) by calling report_complete().
   If an intervening call to report() occurs when a partially constructed
   message exists, then, in an attempt to keep the messages in their proper
   sequence, the partial message will be printed as-is (with a trailing 
   newline) before report() prints its message. */
/* VARARGS */

static void rep_ensuresize(void) {
    /* Make an initial guess for the size of any single message fragment.  */
    if (partial_message_size == 0)
    {
	partial_message_size_used = 0;
	partial_message_size = 2048;
	partial_message = (char *)MALLOC (partial_message_size);
    }
    else
	if (partial_message_size - partial_message_size_used < 1024)
	{
	    partial_message_size += 2048;
	    partial_message = (char *)REALLOC (partial_message, partial_message_size);
	}
}

#ifdef HAVE_STDARG_H
static void report_vbuild(const char *message, va_list args)
{
    int n;

    for ( ; ; )
    {
	/*
	 * args has to be initialized before every call of vsnprintf(), 
	 * because vsnprintf() invokes va_arg macro and thus args is 
	 * undefined after the call.
	 */
	n = vsnprintf (partial_message + partial_message_size_used, partial_message_size - partial_message_size_used,
		       message, args);

	/* output error, f. i. EILSEQ */
	if (n < 0) break;

	if (n >= 0
	    && (unsigned)n < partial_message_size - partial_message_size_used)
        {
	    partial_message_size_used += n;
	    break;
	}

	partial_message_size += 2048;
	partial_message = (char *)REALLOC (partial_message, partial_message_size);
    }
}
#endif

void
#ifdef HAVE_STDARG_H
report_build (FILE *errfp, const char *message, ...)
#else
report_build (FILE *errfp, message, va_alist)
     const char *message;
     va_dcl
#endif
{
#ifdef VA_START
    va_list args;
#else
    int n;
#endif

    rep_ensuresize();

#if defined(VA_START)
    VA_START(args, message);
    report_vbuild(message, args);
    va_end(args);
#else
    for ( ; ; )
    {
	n = snprintf (partial_message + partial_message_size_used,
		      partial_message_size - partial_message_size_used,
		      message, a1, a2, a3, a4, a5, a6, a7, a8);

	/* output error, f. i. EILSEQ */
	if (n < 0) break;

	if (n >= 0
	    && (unsigned)n < partial_message_size - partial_message_size_used)
        {
	    partial_message_size_used += n;
	    break;
	}

	partial_message_size += 2048;
	partial_message = REALLOC (partial_message, partial_message_size);
    }
#endif

    if (unbuffered && partial_message_size_used != 0)
    {
	partial_message_size_used = 0;
	fputs(partial_message, errfp);
    }
}

void report_flush(FILE *errfp)
{
    if (partial_message_size_used != 0)
    {
	partial_message_size_used = 0;
	report(errfp, "%s", partial_message);
	partial_suppress_tag = 1;
    }
}

/* Complete a report message by appending MESSAGE, which is a printf-style
   format string with optional args, to the existing report message (which may
   be empty.)  The completed report message is then printed (and reset to
   empty.) */
/* VARARGS */
void
#ifdef HAVE_STDARG_H
report_complete (FILE *errfp, const char *message, ...)
#else
report_complete (FILE *errfp, message, va_alist)
     const char *message;
     va_dcl
#endif
{
#ifdef VA_START
    va_list args;
#endif

    rep_ensuresize();

#if defined(VA_START)
    VA_START(args, message);
    report_vbuild(message, args);
    va_end(args);
#else
    report_build(errfp, message, a1, a2, a3, a4, a5, a6, a7, a8);
#endif

    /* Finally... print it.  */
    partial_message_size_used = 0;

    if (unbuffered)
    {
	fputs(partial_message, errfp);
	fflush (errfp);
    }
    else
	report(errfp, "%s", partial_message);
}

/* Sometimes we want to have at most one error per line.  This
   variable controls whether this mode is selected or not.  */
static int error_one_per_line;

/* If errnum is nonzero, print its corresponding system error message. */
void
#ifdef HAVE_STDARG_H
report_at_line (FILE *errfp, int errnum, const char *file_name,
	       unsigned int line_number, const char *message, ...)
#else
report_at_line (FILE *errfp, errnum, file_name, line_number, message, va_alist)
     int errnum;
     const char *file_name;
     unsigned int line_number;
     const char *message;
     va_dcl
#endif
{
#ifdef VA_START
    va_list args;
#endif

    if (error_one_per_line)
    {
	static const char *old_file_name;
	static unsigned int old_line_number;

	if (old_line_number == line_number &&
	    (file_name == old_file_name || !strcmp (old_file_name, file_name)))
	    /* Simply return and print nothing.  */
	    return;

	old_file_name = file_name;
	old_line_number = line_number;
    }

    fflush (errfp);
    if ( *message == '\n' )
    {
	fputc( '\n', errfp );
	++message;
    }
    fprintf (errfp, "%s:", program_name);

    if (file_name != NULL)
	fprintf (errfp, "%s:%u: ", file_name, line_number);

#ifdef VA_START
    VA_START (args, message);
# if defined(HAVE_VPRINTF) || defined(_LIBC)
    vfprintf (errfp, message, args);
# else
    _doprnt (message, args, errfp);
# endif
    va_end (args);
#else
    fprintf (errfp, message, a1, a2, a3, a4, a5, a6, a7, a8);
#endif

    if (errnum)
	fprintf (errfp, ": %s", strerror (errnum));
    putc ('\n', errfp);
    fflush (errfp);
}
