/* error.c -- error handler for noninteractive utilities
   Copyright (C) 1990, 91, 92, 93, 94, 95, 96 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by David MacKenzie <djm@gnu.ai.mit.edu>.
 * Heavily modified by Dave Bodenstab and ESR.
 * Bludgeoned into submission for SunOS 4.1.3 by
 *     Chris Cheyney <cheyney@netcom.com>.
 * Now it works even when the return from vprintf is unreliable.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#if defined(HAVE_SYSLOG)
#include <syslog.h>
#endif

#if HAVE_VPRINTF || HAVE_DOPRNT || _LIBC || HAVE_STDARG_H
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

#if STDC_HEADERS || _LIBC
# include <stdlib.h>
# include <string.h>
#else
void exit ();
#endif

#include "i18n.h"

#include "fetchmail.h"
#define MALLOC(n)	xmalloc(n)	
#define REALLOC(n,s)	xrealloc(n,s)	

/* If NULL, error will flush stderr, then print on stderr the program
   name, a colon and a space.  Otherwise, error will call this
   function without parameters instead.  */
void (*error_print_progname) (
#if __STDC__ - 0
			      void
#endif
			      );

/* Used by error_build() and error_complete() to accumulate partial messages.  */
static unsigned int partial_message_size = 0;
static unsigned int partial_message_size_used = 0;
static char *partial_message;
static unsigned use_stderr;
static unsigned int use_syslog;

/* This variable is incremented each time `error' is called.  */
unsigned int error_message_count;

#ifdef _LIBC
/* In the GNU C library, there is a predefined variable for this.  */

# define program_name program_invocation_name
# include <errno.h>

#else

/* The calling program should define program_name and set it to the
   name of the executing program.  */
extern char *program_name;

# if !HAVE_STRERROR && !defined(strerror)
char *strerror (errnum)
     int errnum;
{
  extern char *sys_errlist[];
  extern int sys_nerr;

  if (errnum > 0 && errnum <= sys_nerr)
    return sys_errlist[errnum];
  return _("Unknown system error");
}
# endif	/* HAVE_STRERROR */
#endif	/* _LIBC */

/* Print the program name and error message MESSAGE, which is a printf-style
   format string with optional args.
   If ERRNUM is nonzero, print its corresponding system error message. */
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
      report (errfp, 0, _("%s (log message incomplete)"), partial_message);
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
  else
#endif
    {
      if (error_print_progname)
	(*error_print_progname) ();
      else
	{
	  fflush (errfp);
	  if ( *message == '\n' )
	    {
	      fputc( '\n', errfp );
	      ++message;
	    }
	  fprintf (errfp, "%s: ", program_name);
	}

#ifdef VA_START
      VA_START (args, message);
# if HAVE_VPRINTF || _LIBC
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
  ++error_message_count;
}

/*
 * Calling report_init(1) causes error_build and error_complete to write
 * to errfp without buffering.  This is needed for the ticker dots to
 * work correctly.
 */
void report_init(int mode)
{
    switch(mode)
    {
    case 0:			/* errfp, buffered */
    default:
	use_stderr = FALSE;
	use_syslog = FALSE;
	break;

    case 1:			/* errfp, unbuffered */
	use_stderr = TRUE;
	use_syslog = FALSE;
	break;

#ifdef HAVE_SYSLOG
    case -1:			/* syslogd */
	use_stderr = FALSE;
	use_syslog = TRUE;
	break;
#endif /* HAVE_SYSLOG */
    }
}

/* Build an error message by appending MESSAGE, which is a printf-style
   format string with optional args, to the existing error message (which may
   be empty.)  The completed error message is finally printed (and reset to
   empty) by calling error_complete().
   If an intervening call to report() occurs when a partially constructed
   message exists, then, in an attempt to keep the messages in their proper
   sequence, the partial message will be printed as-is (with a trailing 
   newline) before report() prints its message. */
/* VARARGS */

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
  int n;
#endif

  /* Make an initial guess for the size of any single message fragment.  */
  if (partial_message_size == 0)
    {
      partial_message_size_used = 0;
      partial_message_size = 2048;
      partial_message = MALLOC (partial_message_size);
    }
  else
    if (partial_message_size - partial_message_size_used < 1024)
      {
        partial_message_size += 2048;
        partial_message = REALLOC (partial_message, partial_message_size);
      }

#if defined(VA_START)
  VA_START (args, message);
#if HAVE_VSNPRINTF || _LIBC
  for ( ; ; )
    {
      n = vsnprintf (partial_message + partial_message_size_used,
		     partial_message_size - partial_message_size_used,
		     message, args);

      if (n < partial_message_size - partial_message_size_used)
        {
	  partial_message_size_used += n;
	  break;
	}

      partial_message_size += 2048;
      partial_message = REALLOC (partial_message, partial_message_size);
    }
#else
  vsprintf (partial_message + partial_message_size_used, message, args);
  partial_message_size_used += strlen(partial_message+partial_message_size_used);

  /* Attempt to catch memory overwrites... */
  if (partial_message_size_used >= partial_message_size)
    {
      partial_message_size_used = 0;
      report (stderr, _("partial error message buffer overflow"));
    }
#endif
  va_end (args);
#else
#if HAVE_SNPRINTF
  for ( ; ; )
    {
      n = snprintf (partial_message + partial_message_size_used,
		    partial_message_size - partial_message_size_used,
		    message, a1, a2, a3, a4, a5, a6, a7, a8);

      if (n < partial_message_size - partial_message_size_used)
        {
	  partial_message_size_used += n;
	  break;
	}

      partial_message_size += 2048;
      partial_message = REALLOC (partial_message, partial_message_size);
    }
#else
  sprintf (partial_message + partial_message_size_used, message, a1, a2, a3, a4, a5, a6, a7, a8);

  /* Attempt to catch memory overwrites... */
  if ((partial_message_size_used = strlen (partial_message)) >= partial_message_size)
    {
      partial_message_size_used = 0;
      report (stderr, _("partial error message buffer overflow"));
    }
#endif
#endif

  if (use_stderr && partial_message_size_used != 0)
    {
      partial_message_size_used = 0;
      fputs(partial_message, errfp);
    }
}

/* Complete an error message by appending MESSAGE, which is a printf-style
   format string with optional args, to the existing error message (which may
   be empty.)  The completed error message is then printed (and reset to
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
  int n;
#endif

  /* Make an initial guess for the size of any single message fragment.  */
  if (partial_message_size == 0)
    {
      partial_message_size_used = 0;
      partial_message_size = 2048;
      partial_message = MALLOC (partial_message_size);
    }
  else
    if (partial_message_size - partial_message_size_used < 1024)
      {
        partial_message_size += 2048;
        partial_message = REALLOC (partial_message, partial_message_size);
      }

#if defined(VA_START)
  VA_START (args, message);
#if HAVE_VSNPRINTF || _LIBC
  for ( ; ; )
    {
      n = vsnprintf (partial_message + partial_message_size_used,
		     partial_message_size - partial_message_size_used,
		     message, args);

      if (n < partial_message_size - partial_message_size_used)
        {
	  partial_message_size_used += n;
	  break;
	}

      partial_message_size += 2048;
      partial_message = REALLOC (partial_message, partial_message_size);
    }
#else
  vsprintf (partial_message + partial_message_size_used, message, args);
  partial_message_size_used += strlen(partial_message+partial_message_size_used);

  /* Attempt to catch memory overwrites... */
  if (partial_message_size_used >= partial_message_size)
    {
      partial_message_size_used = 0;
      report (stderr, _("partial error message buffer overflow"));
    }
#endif
  va_end (args);
#else
#if HAVE_SNPRINTF
  for ( ; ; )
    {
      n = snprintf (partial_message + partial_message_size_used,
		    partial_message_size - partial_message_size_used,
		    message, a1, a2, a3, a4, a5, a6, a7, a8);

      if (n < partial_message_size - partial_message_size_used)
        {
	  partial_message_size_used += n;
	  break;
	}

      partial_message_size += 2048;
      partial_message = REALLOC (partial_message, partial_message_size);
    }
#else
  sprintf (partial_message + partial_message_size_used, message, a1, a2, a3, a4, a5, a6, a7, a8);

  /* Attempt to catch memory overwrites... */
  if ((partial_message_size_used = strlen (partial_message)) >= partial_message_size)
    {
      partial_message_size_used = 0;
      report (stderr, _("partial error message buffer overflow"));
    }
#endif
#endif

  /* Finally... print it.  */
  partial_message_size_used = 0;

  if (use_stderr)
    {
      fputs(partial_message, errfp);
      fflush (errfp);

      ++error_message_count;
    }
  else
    report(errfp, "%s", partial_message);
}

/* Sometimes we want to have at most one error per line.  This
   variable controls whether this mode is selected or not.  */
int error_one_per_line;

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

  if (error_print_progname)
    (*error_print_progname) ();
  else
    {
      fflush (errfp);
      if ( *message == '\n' )
	{
	  fputc( '\n', errfp );
	  ++message;
	}
      fprintf (errfp, "%s:", program_name);
    }

  if (file_name != NULL)
    fprintf (errfp, "%s:%d: ", file_name, line_number);

#ifdef VA_START
  VA_START (args, message);
# if HAVE_VPRINTF || _LIBC
  vfprintf (errfp, message, args);
# else
  _doprnt (message, args, errfp);
# endif
  va_end (args);
#else
  fprintf (errfp, message, a1, a2, a3, a4, a5, a6, a7, a8);
#endif

  ++error_message_count;
  if (errnum)
    fprintf (errfp, ": %s", strerror (errnum));
  putc ('\n', errfp);
  fflush (errfp);
}
