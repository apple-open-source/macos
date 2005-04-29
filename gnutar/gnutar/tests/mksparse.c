/* This file is part of GNU tar test suite

   Copyright (C) 2004 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>

char *progname;
char *buffer;
size_t buffer_size;

static void die (char const *, ...) __attribute__ ((noreturn,
						    format (printf, 1, 2)));
static void
die (char const *fmt, ...)
{
  va_list ap;

  fprintf (stderr, "%s: ", progname);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fprintf (stderr, "\n");
  exit (1);
}

static void
mkhole (int fd, off_t displ)
{
  if (lseek (fd, displ, SEEK_CUR) == -1)
    {
      perror ("lseek");
      exit (1);
    }
  ftruncate (fd, lseek (fd, 0, SEEK_CUR));
}

static void
mksparse (int fd, off_t displ, char *marks)
{
  for (; *marks; marks++)
    {
      memset (buffer, *marks, buffer_size);
      if (write(fd, buffer, buffer_size) != buffer_size)
	{
	  perror ("write");
	  exit (1);
	}

      if (lseek (fd, displ, SEEK_CUR) == -1)
	{
	  perror ("lseek");
	  exit (1);
	}
    }
}

static void usage (void) __attribute__ ((noreturn));
static void
usage (void)
{
  printf ("Usage: mksparse filename blocksize disp letters [disp letters...] [disp]\n");
  exit (1);
}

static int
xlat_suffix (off_t *vp, char *p)
{
  if (p[1])
    return 1;
  switch (p[0])
    {
    case 'g':
    case 'G':
      *vp *= 1024;

    case 'm':
    case 'M':
      *vp *= 1024;

    case 'k':
    case 'K':
      *vp *= 1024;
      break;

    default:
      return 1;
    }
  return 0;
}

int
main (int argc, char **argv)
{
  int i;
  int fd;
  char *p;
  off_t n;

  progname = strrchr (argv[0], '/');
  if (progname)
    progname++;
  else
    progname = argv[0];

  if (argc < 4)
    usage ();

  fd = open (argv[1], O_CREAT|O_TRUNC|O_RDWR, 0644);
  if (fd < 0)
    die ("cannot open %s", argv[1]);

  n = strtoul (argv[2], &p, 0);
  if (n <= 0 || (*p && xlat_suffix (&n, p)))
    die ("Invalid buffer size: %s", argv[2]);
  buffer_size = n;
  buffer = malloc (buffer_size);
  if (!buffer)
    die ("Not enough memory");

  for (i = 3; i < argc; i += 2)
    {
      off_t displ;

      displ = strtoul (argv[i], &p, 0);
      if (displ < 0 || (*p && xlat_suffix (&displ, p)))
	die ("Invalid displacement: %s", argv[i]);

      if (i == argc-1)
	{
	  mkhole (fd, displ);
	  break;
	}
      else
	mksparse (fd, displ, argv[i+1]);
    }

  close(fd);
  return 0;
}
