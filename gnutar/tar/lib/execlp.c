/* Provide an execlp replacement for Minix.
   Copyright (C) 1988, 1994, 1995, 1997 Free Software Foundation, Inc.

   This file is part of GNU Tar.

   GNU Tar is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GNU Tar is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Tar; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

/* For defining NULL.  */
#include <stdio.h>

#if STDC_HEADERS
# include <stdlib.h>
#else
char *getenv ();
char *malloc ();
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#include <sys/types.h>
#include <sys/stat.h>

#if STDC_HEADERS || HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
# ifndef strchr
#  define strchr index
# endif
#endif

/* Synopsis: execlp (file, arg0, arg1... argN, (char *) NULL)

   Exec a program, automatically searching for the program through all the
   directories on the PATH.

   This version is naive about variable argument lists and assumes a quite
   straightforward C calling sequence.  It will not work on systems having
   odd stacks.  */

int
execlp (filename, arg0)
     char *filename;
     char *arg0;
{
  register char *p, *path;
  register char *fnbuffer;
  char **argstart = &arg0;
  struct stat statbuf;
  extern char **environ;

  if (p = getenv ("PATH"), p == NULL)
    {
      /* Could not find path variable -- try to exec given filename.  */

      return execve (filename, argstart, environ);
    }

  /* Make a place to build the filename.  We malloc larger than we need,
     but we know it will fit in this.  */

  fnbuffer = malloc (strlen (p) + 1 + strlen (filename));
  if (fnbuffer == NULL)
    {
      errno = ENOMEM;
      return -1;
    }

  /* Try each component of the path to see if the file's there and
     executable.  */

  for (path = p; path; path = p)
    {

      /* Construct full path name to try.  */

      if (p = strchr (path, ':'), !p)
	strcpy (fnbuffer, path);
      else
	{
	  strncpy (fnbuffer, path, p - path);
	  fnbuffer[p - path] = '\0';
	  p++;			/* skip : for next time */
	}
      if (strlen (fnbuffer) != 0)
	strcat (fnbuffer, "/");
      strcat (fnbuffer, filename);

      /* Check to see if file is there and is a normal file.  */

      if (stat (fnbuffer, &statbuf) < 0)
	{
	  if (errno == ENOENT)
	    continue;		/* file not there,keep on looking */
	  else
	    goto fail;		/* failed for some reason, return */
	}
      if (!S_ISREG (statbuf.st_mode))
	continue;

      if (execve (fnbuffer, argstart, environ) < 0
	  && errno != ENOENT
	  && errno != ENOEXEC)
	{
	  /* Failed, for some other reason besides "file.  */

	  goto fail;
	}

      /* If we got error ENOEXEC, the file is executable but is not an
	 object file.  Try to execute it as a shell script, returning
	 error if we can't execute /bin/sh.

	 FIXME, this code is broken in several ways.  Shell scripts
	 should not in general be executed by the user's SHELL variable
	 program.  On more mature systems, the script can specify with
	 #!/bin/whatever.  Also, this code clobbers argstart[-1] if the
	 exec of the shell fails.  */

      if (errno == ENOEXEC)
	{
	  char *shell;

	  /* Try to execute command "sh arg0 arg1 ...".  */

	  if (shell = getenv ("SHELL"), shell == NULL)
	    shell = "/bin/sh";
	  argstart[-1] = shell;
	  argstart[0] = fnbuffer;
	  execve (shell, &argstart[-1], environ);
	  goto fail;		/* exec didn't work */
	}

      /* If we succeeded, the execve() doesn't return, so we can only be
	 here is if the file hasn't been found yet.  Try the next place
	 on the path.  */

    }

  /* All attempts failed to locate the file.  Give up.  */

  errno = ENOENT;

fail:
  free (fnbuffer);
  return -1;
}
