/* Remote connection server.
   Copyright 1994, 1995, 1996, 1997, 1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Copyright (C) 1983 Regents of the University of California.
   All rights reserved.

   Redistribution and use in source and binary forms are permitted provided
   that the above copyright notice and this paragraph are duplicated in all
   such forms and that any documentation, advertising materials, and other
   materials related to such distribution and use acknowledge that the
   software was developed by the University of California, Berkeley.  The
   name of the University may not be used to endorse or promote products
   derived from this software without specific prior written permission.
   THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
   MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.  */

#include "system.h"
#include "safe-read.h"

#include <sys/socket.h>

#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif
#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif

/* Maximum size of a string from the requesting program.  */
#define	STRING_SIZE 64

/* Name of executing program.  */
const char *program_name;

/* File descriptor of the tape device, or negative if none open.  */
static int tape = -1;

/* Buffer containing transferred data, and its allocated size.  */
static char *record_buffer = NULL;
static size_t allocated_size = 0;

/* Buffer for constructing the reply.  */
static char reply_buffer[BUFSIZ];

/* Debugging tools.  */

static FILE *debug_file = NULL;

#define	DEBUG(File) \
  if (debug_file) fprintf(debug_file, File)

#define	DEBUG1(File, Arg) \
  if (debug_file) fprintf(debug_file, File, Arg)

#define	DEBUG2(File, Arg1, Arg2) \
  if (debug_file) fprintf(debug_file, File, Arg1, Arg2)

/*------------------------------------------------.
| Return an error string, given an error number.  |
`------------------------------------------------*/

#if HAVE_STRERROR
# ifndef strerror
char *strerror ();
# endif
#else
static char *
private_strerror (int errnum)
{
  extern char *sys_errlist[];
  extern int sys_nerr;

  if (errnum > 0 && errnum <= sys_nerr)
    return _(sys_errlist[errnum]);
  return _("Unknown system error");
}
# define strerror private_strerror
#endif

/*---.
| ?  |
`---*/

static void
report_error_message (const char *string)
{
  DEBUG1 ("rmtd: E 0 (%s)\n", string);

  sprintf (reply_buffer, "E0\n%s\n", string);
  full_write (STDOUT_FILENO, reply_buffer, strlen (reply_buffer));
}

/*---.
| ?  |
`---*/

static void
report_numbered_error (int num)
{
  DEBUG2 ("rmtd: E %d (%s)\n", num, strerror (num));

  sprintf (reply_buffer, "E%d\n%s\n", num, strerror (num));
  full_write (STDOUT_FILENO, reply_buffer, strlen (reply_buffer));
}

/*---.
| ?  |
`---*/

static void
get_string (char *string)
{
  int counter;

  for (counter = 0; counter < STRING_SIZE; counter++)
    {
      if (safe_read (STDIN_FILENO, string + counter, 1) != 1)
	exit (EXIT_SUCCESS);

      if (string[counter] == '\n')
	break;
    }
  string[counter] = '\0';
}

/*---.
| ?  |
`---*/

static void
prepare_record_buffer (size_t size)
{
  if (size <= allocated_size)
    return;

  if (record_buffer)
    free (record_buffer);

  record_buffer = malloc (size);

  if (record_buffer == NULL)
    {
      DEBUG (_("rmtd: Cannot allocate buffer space\n"));

      report_error_message (N_("Cannot allocate buffer space"));
      exit (EXIT_FAILURE);      /* exit status used to be 4 */
    }

  allocated_size = size;

#ifdef SO_RCVBUF
  while (size > 1024 &&
	 (setsockopt (STDIN_FILENO, SOL_SOCKET, SO_RCVBUF,
		      (char *) &size, sizeof size)
	  < 0))
    size -= 1024;
#else
  /* FIXME: I do not see any purpose to the following line...  Sigh! */
  size = 1 + ((size - 1) % 1024);
#endif
}

/*---.
| ?  |
`---*/

int
main (int argc, char *const *argv)
{
  char command;
  long status;

  /* FIXME: Localisation is meaningless, unless --help and --version are
     locally used.  Localisation would be best accomplished by the calling
     tar, on messages found within error packets.  */

  program_name = argv[0];
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  /* FIXME: Implement --help and --version as for any other GNU program.  */

  argc--, argv++;
  if (argc > 0)
    {
      debug_file = fopen (*argv, "w");
      if (debug_file == 0)
	{
	  report_numbered_error (errno);
	  exit (EXIT_FAILURE);
	}
      setbuf (debug_file, NULL);
    }

top:
  errno = 0;			/* FIXME: errno should be read-only */
  status = 0;
  if (safe_read (STDIN_FILENO, &command, 1) != 1)
    exit (EXIT_SUCCESS);

  switch (command)
    {
      /* FIXME: Maybe 'H' and 'V' for --help and --version output?  */

    case 'O':
      {
	char device_string[STRING_SIZE];
	char mode_string[STRING_SIZE];

	get_string (device_string);
	get_string (mode_string);
	DEBUG2 ("rmtd: O %s %s\n", device_string, mode_string);

	if (tape >= 0)
	  close (tape);

	tape = open (device_string, atoi (mode_string), 0666);
	if (tape < 0)
	  goto ioerror;
	goto respond;
      }

    case 'C':
      {
	char device_string[STRING_SIZE];

	get_string (device_string); /* discard */
	DEBUG ("rmtd: C\n");

	if (close (tape) < 0)
	  goto ioerror;
	tape = -1;
	goto respond;
      }

    case 'L':
      {
	char count_string[STRING_SIZE];
	char position_string[STRING_SIZE];
	off_t count = 0;
	int negative;
	int whence;
	char *p;

	get_string (count_string);
	get_string (position_string);
	DEBUG2 ("rmtd: L %s %s\n", count_string, position_string);

	/* Parse count_string, taking care to check for overflow.
	   We can't use standard functions,
	   since off_t might be longer than long.  */

	for (p = count_string;  *p == ' ' || *p == '\t';  p++)
	  continue;

	negative = *p == '-';
	p += negative || *p == '+';

	for (;;)
	  {
	    int digit = *p++ - '0';
	    if (9 < (unsigned) digit)
	      break;
	    else
	      {
		off_t c10 = 10 * count;
		off_t nc = negative ? c10 - digit : c10 + digit;
		if (c10 / 10 != count || (negative ? c10 < nc : nc < c10))
		  {
		    report_error_message (N_("Seek offset out of range"));
		    exit (EXIT_FAILURE);
		  }
		count = nc;
	      }
	  }

	switch (atoi (position_string))
	  {
	  case 0: whence = SEEK_SET; break;
	  case 1: whence = SEEK_CUR; break;
	  case 2: whence = SEEK_END; break;
	  default:
	    report_error_message (N_("Seek direction out of range"));
	    exit (EXIT_FAILURE);
	  }
	count = lseek (tape, count, whence);
	if (count < 0)
	  goto ioerror;

	/* Convert count back to string for reply.
	   We can't use sprintf, since off_t might be longer than long.  */
	p = count_string + sizeof count_string;
	*--p = '\0';
	do
	  *--p = '0' + (int) (count % 10);
	while ((count /= 10) != 0);
	
	DEBUG1 ("rmtd: A %s\n", p);

	sprintf (reply_buffer, "A%s\n", p);
	full_write (STDOUT_FILENO, reply_buffer, strlen (reply_buffer));
	goto top;
      }

    case 'W':
      {
	char count_string[STRING_SIZE];
	size_t size;
	size_t counter;

	get_string (count_string);
	size = atol (count_string);
	DEBUG1 ("rmtd: W %s\n", count_string);

	prepare_record_buffer (size);
	for (counter = 0; counter < size; counter += status)
	  {
	    status = safe_read (STDIN_FILENO, &record_buffer[counter],
				size - counter);
	    if (status <= 0)
	      {
		DEBUG (_("rmtd: Premature eof\n"));

		report_error_message (N_("Premature end of file"));
		exit (EXIT_FAILURE); /* exit status used to be 2 */
	      }
	  }
	status = full_write (tape, record_buffer, size);
	if (status < 0)
	  goto ioerror;
	goto respond;
      }

    case 'R':
      {
	char count_string[STRING_SIZE];
	size_t size;

	get_string (count_string);
	DEBUG1 ("rmtd: R %s\n", count_string);

	size = atol (count_string);
	prepare_record_buffer (size);
	status = safe_read (tape, record_buffer, size);
	if (status < 0)
	  goto ioerror;
	sprintf (reply_buffer, "A%ld\n", status);
	full_write (STDOUT_FILENO, reply_buffer, strlen (reply_buffer));
	full_write (STDOUT_FILENO, record_buffer, (size_t) status);
	goto top;
      }

    case 'I':
      {
	char operation_string[STRING_SIZE];
	char count_string[STRING_SIZE];

	get_string (operation_string);
	get_string  (count_string);
	DEBUG2 ("rmtd: I %s %s\n", operation_string, count_string);

#ifdef MTIOCTOP
	{
	  struct mtop mtop;
	  const char *p;
	  off_t count = 0;
	  int negative;

	  /* Parse count_string, taking care to check for overflow.
	     We can't use standard functions,
	     since off_t might be longer than long.  */
	  
	  for (p = count_string;  *p == ' ' || *p == '\t';  p++)
	    continue;
	  
	  negative = *p == '-';
	  p += negative || *p == '+';
	  
	  for (;;)
	    {
	      int digit = *p++ - '0';
	      if (9 < (unsigned) digit)
		break;
	      else
		{
		  off_t c10 = 10 * count;
		  off_t nc = negative ? c10 - digit : c10 + digit;
		  if (c10 / 10 != count || (negative ? c10 < nc : nc < c10))
		    {
		      report_error_message (N_("Seek offset out of range"));
		      exit (EXIT_FAILURE);
		    }
		  count = nc;
		}
	    }

	  mtop.mt_count = count;
	  if (mtop.mt_count != count)
	    {
	      report_error_message (N_("Seek offset out of range"));
	      exit (EXIT_FAILURE);
	    }
	  mtop.mt_op = atoi (operation_string);

	  if (ioctl (tape, MTIOCTOP, (char *) &mtop) < 0)
	    goto ioerror;
	}
#endif
	goto respond;
      }

    case 'S':			/* status */
      {
	DEBUG ("rmtd: S\n");

#ifdef MTIOCGET
	{
	  struct mtget operation;

	  if (ioctl (tape, MTIOCGET, (char *) &operation) < 0)
	    goto ioerror;
	  status = sizeof (operation);
	  sprintf (reply_buffer, "A%ld\n", status);
	  full_write (STDOUT_FILENO, reply_buffer, strlen (reply_buffer));
	  full_write (STDOUT_FILENO, (char *) &operation, sizeof (operation));
	}
#endif
	goto top;
      }

    default:
      DEBUG1 (_("rmtd: Garbage command %c\n"), command);

      report_error_message (N_("Garbage command"));
      exit (EXIT_FAILURE);	/* exit status used to be 3 */
    }

respond:
  DEBUG1 ("rmtd: A %ld\n", status);

  sprintf (reply_buffer, "A%ld\n", status);
  full_write (STDOUT_FILENO, reply_buffer, strlen (reply_buffer));
  goto top;

ioerror:
  report_numbered_error (errno);
  goto top;
}
