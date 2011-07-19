/* help.c, created from help.def. */
#line 23 "help.def"

#line 34 "help.def"

#include <config.h>

#if defined (HELP_BUILTIN)
#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include <errno.h>

#include <filecntl.h>

#include "../bashintl.h"

#include "../shell.h"
#include "../builtins.h"
#include "../pathexp.h"
#include "common.h"
#include "bashgetopt.h"

#include <glob/strmatch.h>
#include <glob/glob.h>

#ifndef errno
extern int errno;
#endif

static void show_builtin_command_help __P((void));
static void show_longdoc __P((int));

/* Print out a list of the known functions in the shell, and what they do.
   If LIST is supplied, print out the list which matches for each pattern
   specified. */
int
help_builtin (list)
     WORD_LIST *list;
{
  register int i;
  char *pattern, *name;
  int plen, match_found, sflag;

  sflag = 0;
  reset_internal_getopt ();
  while ((i = internal_getopt (list, "s")) != -1)
    {
      switch (i)
	{
	case 's':
	  sflag = 1;
	  break;
	default:
	  builtin_usage ();
	  return (EX_USAGE);
	}
    }
  list = loptend;

  if (list == 0)
    {
      show_shell_version (0);
      show_builtin_command_help ();
      return (EXECUTION_SUCCESS);
    }

  /* We should consider making `help bash' do something. */

  if (glob_pattern_p (list->word->word))
    {
      if (list->next)
	printf (_("Shell commands matching keywords `"));
      else
	printf (_("Shell commands matching keyword `"));
      print_word_list (list, ", ");
      printf ("'\n\n");
    }

  for (match_found = 0, pattern = ""; list; list = list->next)
    {
      pattern = list->word->word;
      plen = strlen (pattern);

      for (i = 0; name = shell_builtins[i].name; i++)
	{
	  QUIT;
	  if ((strncmp (pattern, name, plen) == 0) ||
	      (strmatch (pattern, name, FNMATCH_EXTFLAG) != FNM_NOMATCH))
	    {
	      printf ("%s: %s\n", name, shell_builtins[i].short_doc);

	      if (sflag == 0)
		show_longdoc (i);

	      match_found++;
	    }
	}
    }

  if (match_found == 0)
    {
      builtin_error (_("no help topics match `%s'.  Try `help help' or `man -k %s' or `info %s'."), pattern, pattern, pattern);
      return (EXECUTION_FAILURE);
    }

  fflush (stdout);
  return (EXECUTION_SUCCESS);
}

/* By convention, enforced by mkbuiltins.c, if separate help files are being
   used, the long_doc array contains one string -- the full pathname of the
   help file for this builtin.  */
static void
show_longdoc (i)
     int i;
{
  register int j;
  char * const *doc;
  int fd;

  doc = shell_builtins[i].long_doc;

  if (doc && doc[0] && *doc[0] == '/' && doc[1] == (char *)NULL)
    {
      fd = open (doc[0], O_RDONLY);
      if (fd == -1)
	{
	  builtin_error (_("%s: cannot open: %s"), doc[0], strerror (errno));
	  return;
	}
      zcatfd (fd, 1, doc[0]);
      close (fd);
    }
  else
    for (j = 0; doc[j]; j++)
      printf ("%*s%s\n", BASE_INDENT, " ", _(doc[j]));
}

static void
show_builtin_command_help ()
{
  int i, j;
  char blurb[36];

  printf (
_("These shell commands are defined internally.  Type `help' to see this list.\n\
Type `help name' to find out more about the function `name'.\n\
Use `info bash' to find out more about the shell in general.\n\
Use `man -k' or `info' to find out more about commands not in this list.\n\
\n\
A star (*) next to a name means that the command is disabled.\n\
\n"));

  for (i = 0; i < num_shell_builtins; i++)
    {
      QUIT;
      blurb[0] = (shell_builtins[i].flags & BUILTIN_ENABLED) ? ' ' : '*';
      strncpy (blurb + 1, shell_builtins[i].short_doc, 34);
      blurb[35] = '\0';
      printf ("%s", blurb);

      if (i % 2)
	printf ("\n");
      else
	for (j = strlen (blurb); j < 35; j++)
	  putc (' ', stdout);
    }
  if (i % 2)
    printf ("\n");
}
#endif /* HELP_BUILTIN */
