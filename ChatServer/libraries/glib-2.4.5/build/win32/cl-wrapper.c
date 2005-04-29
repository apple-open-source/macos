/* cl-wrapper -- wrapper around the Microsoft C compiler
 * Copyright (C) 2001 Tor Lillqvist
 *
 * This program accepts Unix-style C compiler command line arguments,
 * and runs the Microsoft C compiler (cl) with corresponding arguments.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>

static char *cmdline;
static char **libraries;
static char **libdirs;
static char **objects;
static char *output_executable = NULL, *executable_type = NULL;
static int lib_ix = 0;
static int libdir_ix = 0;
static int object_ix = 0;

static int compileonly = 0, debug = 0, output = 0, verbose = 0, version = 0;
static int nsources = 0;

static void
backslashify (char *p)
{
  while ((p = strchr (p, '/')) != NULL)
    {
      *p = '\\';
      p++;
    }
}

static char *
quote (char *string)
{
  char *result = malloc (strlen (string) * 2);
  char *p = string, *q = result;

  while (*p)
    {
      if (*p == '"')
	*q++ = '\\';
      *q++ = *p;
      p++;
    }
  *q++ = '\0';
  return result;
}

static void
process_argv (int argc,
	      char **argv)
{
  int i;
  char *lastdot;

  for (i = 1; i < argc; i++)
    if (strcmp (argv[i], "-c") == 0)
      {
	compileonly++;
	strcat (cmdline, " -c");
      }
    else if (strncmp (argv[i], "-D", 2) == 0)
      {
	strcat (cmdline, " ");
	strcat (cmdline, quote (argv[i]));
	if (strlen (argv[i]) == 2)
	  {
	    i++;
	    strcat (cmdline, quote (argv[i]));
	  }
      }
    else if (strcmp (argv[i], "-E") == 0)
      strcat (cmdline, " -E");
    else if (strcmp (argv[i], "-g") == 0)
      debug++;
    else if (strncmp (argv[i], "-I", 2) == 0)
      {
	strcat (cmdline, " ");
	backslashify (argv[i]);
	strcat (cmdline, quote (argv[i]));
	if (strlen (argv[i]) == 2)
	  {
	    i++;
	    backslashify (argv[i]);
	    strcat (cmdline, quote (argv[i]));
	  }
      }
    else if (strncmp (argv[i], "-l", 2) == 0)
      libraries[lib_ix++] = argv[i] + 2;
    else if (strncmp (argv[i], "-L", 2) == 0)
      {
	if (strlen (argv[i]) > 2)
	  {
	    backslashify (argv[i]);
	    libdirs[libdir_ix++] = argv[i] + 2;
	  }
	else
	  {
	    i++;
	    backslashify (argv[i]);
	    libdirs[libdir_ix++] = argv[i];
	  }
      }
    else if (strcmp (argv[i], "-o") == 0)
      {
	output++;
	i++;
	lastdot = strrchr (argv[i], '.');
	if (lastdot != NULL && (stricmp (lastdot, ".exe") == 0
				|| stricmp (lastdot, ".dll") == 0))
	  {
	    strcat (cmdline, " -Fe");
	    backslashify (argv[i]);
	    strcat (cmdline, argv[i]);
	    output_executable = argv[i];
	    executable_type = strrchr (output_executable, '.');
	  }
	else if (lastdot != NULL && (strcmp (lastdot, ".obj") == 0))
	  {
	    strcat (cmdline, " -Fo");
	    strcat (cmdline, argv[i]);
	  }
	else
	  {
	    strcat (cmdline, " -Fe");
	    strcat (cmdline, argv[i]);
	    strcat (cmdline, ".exe");
	  }
      }
    else if (strncmp (argv[i], "-O", 2) == 0)
      strcat (cmdline, " -O2");
    else if (strcmp (argv[i], "-v") == 0)
      verbose++;
    else if (strcmp (argv[i], "--version") == 0)
      version = 1;
    else if (argv[i][0] == '-')
      fprintf (stderr, "Ignored flag %s\n", argv[i]);
    else
      {
	lastdot = strrchr (argv[i], '.');
	if (lastdot != NULL && (stricmp (lastdot, ".c") == 0
				|| stricmp (lastdot, ".cpp") == 0
				|| stricmp (lastdot, ".cc") == 0))
	  {
	    nsources++;
	    strcat (cmdline, " ");
	    if (stricmp (lastdot, ".cc") == 0)
	      strcat (cmdline, "-Tp");
	    strcat (cmdline, argv[i]);
	  }
	else if (lastdot != NULL && stricmp (lastdot, ".obj") == 0)
	  objects[object_ix++] = argv[i];
	else
	  fprintf (stderr, "Ignored argument: %s\n", argv[i]);
      }
}

static void
process_envvar (char *envvar)
{
  int argc = 0;
  char **argv;
  char *p = envvar;

  while (p && *p)
    {
      p = strchr (p, ' ');
      argc++;
      if (p)
	while (*p == ' ')
	  p++;
    }

  argv = malloc (argc * sizeof (char *));

  p = envvar;
  argc = 0;
  while (p && *p)
    {
      char *q = strchr (p, ' ');
      if (q)
	*q = 0;
      argv[argc++] = p;
      if (q)
	{
	  while (*q == ' ')
	    q++;
	}
      p = q;
    }
  process_argv (argc, argv);
}

int
main (int argc,
      char **argv)
{
  int retval;

  int i, j, k;
  char *p;

  libraries = malloc (argc * sizeof (char *));
  libdirs = malloc ((argc+10) * sizeof (char *));
  objects = malloc (argc * sizeof (char *));

  for (k = 0, i = 1; i < argc; i++)
    k += strlen (argv[i]);

  k += 500 + argc;
  cmdline = malloc (k);

  /* -MD: Use msvcrt.dll runtime */
  strcpy (cmdline, "cl -MD");

  p = getenv ("CPPFLAGS");
  if (p && *p)
    process_envvar (p);

  p = getenv ("LDFLAGS");
  if (p && *p)
    process_envvar (p);

  process_argv (argc, argv);

  if (version)
    strcat (cmdline, " -c nul.c");
  else
    {
      if (!verbose)
	strcat (cmdline, " -nologo");

      if (output_executable != NULL)
	{
	  if (stricmp (executable_type, ".dll") == 0)
	    strcat (cmdline, " -LD");
	}

      if (debug)
	strcat (cmdline, " -Zi");

      if (nsources == 0)
	{
	  FILE *dummy = fopen ("__dummy__.c", "w");
	  fprintf (dummy, "static int foobar = 42;\n");
	  fclose (dummy);

	  strcat (cmdline, " __dummy__.c");
	}

      if (!output && !compileonly)
	strcat (cmdline, " -Fea.exe");

      if (!compileonly)
	{
	  strcat (cmdline, " -link");

	  for (i = 0; i < object_ix; i++)
	    {
	      strcat (cmdline, " ");
	      strcat (cmdline, objects[i]);
	    }

	  for (i = 0; i < lib_ix; i++)
	    {
	      strcat (cmdline, " ");
	      for (j = 0; j < libdir_ix; j++)
		{
		  char b[1000];

		  sprintf (b, "%s\\%s.lib", libdirs[j], libraries[i]);
		  if (access (b, 4) == 0)
		    {
		      strcat (cmdline, b);
		      break;
		    }
		  sprintf (b, "%s\\lib%s.lib", libdirs[j], libraries[i]);
		  if (access (b, 4) == 0)
		    {
		      strcat (cmdline, b);
		      break;
		    }
		}

	      if (j == libdir_ix)
		{
		  strcat (cmdline, libraries[i]);
		  strcat (cmdline, ".lib");
		}
	    }
	}
    }
  
  fprintf (stderr, "%s\n", cmdline);
      
  retval = system (cmdline);

  if (nsources == 0)
    remove ("__dummy__.c");

  return retval;
}
