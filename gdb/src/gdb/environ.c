/* environ.c -- library for manipulating environments for GNU.

   Copyright 1986, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 2000, 2005
   2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#include "defs.h"
#include "environ.h"
#include "gdb_string.h"


/* Return a new environment object.  */

struct gdb_environ *
make_environ (void)
{
  struct gdb_environ *e;

  e = (struct gdb_environ *) xmalloc (sizeof (struct gdb_environ));

  e->allocated = 10;
  e->vector = (char **) xmalloc ((e->allocated + 1) * sizeof (char *));
  e->vector[0] = 0;
  return e;
}

/* Free an environment and all the strings in it.  */

void
free_environ (struct gdb_environ *e)
{
  char **vector = e->vector;

  while (*vector)
    xfree (*vector++);

  xfree (e);
}

/* Copy the environment given to this process into E.
   Also copies all the strings in it, so we can be sure
   that all strings in these environments are safe to free.  */

void
init_environ (struct gdb_environ *e)
{
  extern char **environ;
  int i;

  if (environ == NULL)
    return;

  for (i = 0; environ[i]; i++) /*EMPTY */ ;

  if (e->allocated < i)
    {
      e->allocated = max (i, e->allocated + 10);
      e->vector = (char **) xrealloc ((char *) e->vector,
				      (e->allocated + 1) * sizeof (char *));
    }

  memcpy (e->vector, environ, (i + 1) * sizeof (char *));

  while (--i >= 0)
    {
      int len = strlen (e->vector[i]);
      char *new = (char *) xmalloc (len + 1);
      memcpy (new, e->vector[i], len + 1);
      e->vector[i] = new;
    }
}

/* APPLE LOCAL: gdb has to run setgid on MacOS X and
   dyld truncates the DYLD_* environment variables on set[ug]id
   binaries so their environment isn't mucked with by an evil user.

   To work around this, the /usr/bin/gdb shell script copies DYLD_*
   into GDB_DYLD_*, and in this function we look for DYLD_* env
   vars that have been truncated and a matching GDB_DYLD_* env var.
   If this exists, we copy the GDB_DYLD_* value into DYLD_* so that
   it will take effect when the inferior process is run. */

struct dyld_smuggle_pairs {
  const char *real_name;
  const char *smuggled_name;
};

void
smuggle_dyld_settings (struct gdb_environ *e)
{
  /* The list of DYLD_* names was gleaned from dyld's src/dyld.cpp.  */

  struct dyld_smuggle_pairs env_names[] = { 
       {"DYLD_FRAMEWORK_PATH", "GDB_DYLD_FRAMEWORK_PATH"},
       {"DYLD_FALLBACK_FRAMEWORK_PATH", "GDB_DYLD_FALLBACK_FRAMEWORK_PATH"},
       {"DYLD_LIBRARY_PATH", "GDB_DYLD_LIBRARY_PATH"},
       {"DYLD_FALLBACK_LIBRARY_PATH", "GDB_DYLD_FALLBACK_LIBRARY_PATH"},
       {"DYLD_ROOT_PATH", "GDB_DYLD_ROOT_PATH"},
       {"DYLD_PATHS_ROOT", "GDB_DYLD_PATHS_ROOT"},
       {"DYLD_IMAGE_SUFFIX", "GDB_DYLD_IMAGE_SUFFIX"},
       {"DYLD_INSERT_LIBRARIES", "GDB_DYLD_INSERT_LIBRARIES"},
       { NULL, NULL } };
  int i;

  for (i = 0; env_names[i].real_name != NULL; i++)
    {
      const char *real_val = get_in_environ (e, env_names[i].real_name);
      const char *smuggled_val = get_in_environ (e, env_names[i].smuggled_name);

      if (real_val == NULL && smuggled_val == NULL)
        continue;

      if (smuggled_val == NULL)
        continue;

      /* Is the value of the DYLD_* env var truncated to ""? */
      if (real_val != NULL && real_val[0] != '\0')
        continue;

      /* real_val has a value and it looks legitimate - don't overwrite it
         with the smuggled version.  */
      if (real_val != NULL)
        continue;

      set_in_environ (e, env_names[i].real_name, smuggled_val);
    }
}

/* Return the vector of environment E.
   This is used to get something to pass to execve.  */

char **
environ_vector (struct gdb_environ *e)
{
  return e->vector;
}

/* Return the value in environment E of variable VAR.  */

char *
get_in_environ (const struct gdb_environ *e, const char *var)
{
  int len = strlen (var);
  char **vector = e->vector;
  char *s;

  for (; (s = *vector) != NULL; vector++)
    if (strncmp (s, var, len) == 0 && s[len] == '=')
      return &s[len + 1];

  return 0;
}

/* Store the value in E of VAR as VALUE.  */

void
set_in_environ (struct gdb_environ *e, const char *var, const char *value)
{
  int i;
  int len = strlen (var);
  char **vector = e->vector;
  char *s;

  for (i = 0; (s = vector[i]) != NULL; i++)
    if (strncmp (s, var, len) == 0 && s[len] == '=')
      break;

  if (s == 0)
    {
      if (i == e->allocated)
	{
	  e->allocated += 10;
	  vector = (char **) xrealloc ((char *) vector,
				       (e->allocated + 1) * sizeof (char *));
	  e->vector = vector;
	}
      vector[i + 1] = 0;
    }
  else
    xfree (s);

  s = (char *) xmalloc (len + strlen (value) + 2);
  strcpy (s, var);
  strcat (s, "=");
  strcat (s, value);
  vector[i] = s;

  /* This used to handle setting the PATH and GNUTARGET variables
     specially.  The latter has been replaced by "set gnutarget"
     (which has worked since GDB 4.11).  The former affects searching
     the PATH to find SHELL, and searching the PATH to find the
     argument of "symbol-file" or "exec-file".  Maybe we should have
     some kind of "set exec-path" for that.  But in any event, having
     "set env" affect anything besides the inferior is a bad idea.
     What if we want to change the environment we pass to the program
     without afecting GDB's behavior?  */

  return;
}

/* Remove the setting for variable VAR from environment E.  */

void
unset_in_environ (struct gdb_environ *e, char *var)
{
  int len = strlen (var);
  char **vector = e->vector;
  char *s;

  for (; (s = *vector) != NULL; vector++)
    {
      if (DEPRECATED_STREQN (s, var, len) && s[len] == '=')
	{
	  xfree (s);
	  /* Walk through the vector, shuffling args down by one, including
	     the NULL terminator.  Can't use memcpy() here since the regions
	     overlap, and memmove() might not be available. */
	  while ((vector[0] = vector[1]) != NULL)
	    {
	      vector++;
	    }
	  break;
	}
    }
}
