/* msgunfmt - converts binary .mo files to Uniforum style .po files
   Copyright (C) 1995-1998, 2000-2003 Free Software Foundation, Inc.
   Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, April 1995.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include "closeout.h"
#include "error.h"
#include "error-progname.h"
#include "progname.h"
#include "relocatable.h"
#include "basename.h"
#include "exit.h"
#include "message.h"
#include "msgunfmt.h"
#include "read-mo.h"
#include "read-java.h"
#include "read-tcl.h"
#include "write-po.h"
#include "gettext.h"

#define _(str) gettext (str)


/* Be more verbose.  */
bool verbose;

/* Java mode input file specification.  */
static bool java_mode;
static const char *java_resource_name;
static const char *java_locale_name;

/* Tcl mode input file specification.  */
static bool tcl_mode;
static const char *tcl_locale_name;
static const char *tcl_base_directory;

/* Force output of PO file even if empty.  */
static int force_po;

/* Long options.  */
static const struct option long_options[] =
{
  { "escape", no_argument, NULL, 'E' },
  { "force-po", no_argument, &force_po, 1 },
  { "help", no_argument, NULL, 'h' },
  { "indent", no_argument, NULL, 'i' },
  { "java", no_argument, NULL, 'j' },
  { "locale", required_argument, NULL, 'l' },
  { "no-escape", no_argument, NULL, 'e' },
  { "no-wrap", no_argument, NULL, CHAR_MAX + 2 },
  { "output-file", required_argument, NULL, 'o' },
  { "properties-output", no_argument, NULL, 'p' },
  { "resource", required_argument, NULL, 'r' },
  { "sort-output", no_argument, NULL, 's' },
  { "strict", no_argument, NULL, 'S' },
  { "stringtable-output", no_argument, NULL, CHAR_MAX + 3 },
  { "tcl", no_argument, NULL, CHAR_MAX + 1 },
  { "verbose", no_argument, NULL, 'v' },
  { "version", no_argument, NULL, 'V' },
  { "width", required_argument, NULL, 'w', },
  { NULL, 0, NULL, 0 }
};


/* Forward declaration of local functions.  */
static void usage (int status)
#if defined __GNUC__ && ((__GNUC__ == 2 && __GNUC_MINOR__ >= 5) || __GNUC__ > 2)
	__attribute__ ((noreturn))
#endif
;


int
main (int argc, char **argv)
{
  int optchar;
  bool do_help = false;
  bool do_version = false;
  const char *output_file = "-";
  msgdomain_list_ty *result;
  bool sort_by_msgid = false;

  /* Set program name for messages.  */
  set_program_name (argv[0]);
  error_print_progname = maybe_print_progname;

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, relocate (LOCALEDIR));
  textdomain (PACKAGE);

  /* Ensure that write errors on stdout are detected.  */
  atexit (close_stdout);

  while ((optchar = getopt_long (argc, argv, "d:eEhijl:o:pr:svVw:",
				 long_options, NULL))
	 != EOF)
    switch (optchar)
      {
      case '\0':
	/* long option */
	break;

      case 'd':
	tcl_base_directory = optarg;
	break;

      case 'e':
	message_print_style_escape (false);
	break;

      case 'E':
	message_print_style_escape (true);
	break;

      case 'h':
	do_help = true;
	break;

      case 'i':
	message_print_style_indent ();
	break;

      case 'j':
	java_mode = true;
	break;

      case 'l':
	java_locale_name = optarg;
	tcl_locale_name = optarg;
	break;

      case 'o':
	output_file = optarg;
	break;

      case 'p':
	message_print_syntax_properties ();
	break;

      case 'r':
	java_resource_name = optarg;
	break;

      case 's':
	sort_by_msgid = true;
	break;

      case 'S':
	message_print_style_uniforum ();
	break;

      case 'v':
	verbose = true;
	break;

      case 'V':
	do_version = true;
	break;

      case 'w':
	{
	  int value;
	  char *endp;
	  value = strtol (optarg, &endp, 10);
	  if (endp != optarg)
	    message_page_width_set (value);
	}
	break;

      case CHAR_MAX + 1:
	tcl_mode = true;
	break;

      case CHAR_MAX + 2: /* --no-wrap */
	message_page_width_ignore ();
	break;

      case CHAR_MAX + 3: /* --stringtable-output */
	message_print_syntax_stringtable ();
	break;

      default:
	usage (EXIT_FAILURE);
	break;
      }

  /* Version information is requested.  */
  if (do_version)
    {
      printf ("%s (GNU %s) %s\n", basename (program_name), PACKAGE, VERSION);
      /* xgettext: no-wrap */
      printf (_("Copyright (C) %s Free Software Foundation, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"),
	      "1995-1998, 2000-2003");
      printf (_("Written by %s.\n"), "Ulrich Drepper");
      exit (EXIT_SUCCESS);
    }

  /* Help is requested.  */
  if (do_help)
    usage (EXIT_SUCCESS);

  /* Check for contradicting options.  */
  if (java_mode && tcl_mode)
    error (EXIT_FAILURE, 0, _("%s and %s are mutually exclusive"),
	   "--java", "--tcl");
  if (java_mode)
    {
      if (optind < argc)
	{
	  error (EXIT_FAILURE, 0,
		 _("%s and explicit file names are mutually exclusive"),
		 "--java");
	}
    }
  else if (tcl_mode)
    {
      if (optind < argc)
	{
	  error (EXIT_FAILURE, 0,
		 _("%s and explicit file names are mutually exclusive"),
		 "--tcl");
	}
      if (tcl_locale_name == NULL)
	{
	  error (EXIT_SUCCESS, 0,
		 _("%s requires a \"-l locale\" specification"),
		 "--tcl");
	  usage (EXIT_FAILURE);
	}
      if (tcl_base_directory == NULL)
	{
	  error (EXIT_SUCCESS, 0,
		 _("%s requires a \"-d directory\" specification"),
		 "--tcl");
	  usage (EXIT_FAILURE);
	}
    }
  else
    {
      if (java_resource_name != NULL)
	{
	  error (EXIT_SUCCESS, 0, _("%s is only valid with %s"),
		 "--resource", "--java");
	  usage (EXIT_FAILURE);
	}
      if (java_locale_name != NULL)
	{
	  error (EXIT_SUCCESS, 0, _("%s is only valid with %s"),
		 "--locale", "--java");
	  usage (EXIT_FAILURE);
	}
    }

  /* Read the given .mo file. */
  if (java_mode)
    {
      result = msgdomain_read_java (java_resource_name, java_locale_name);
    }
  else if (tcl_mode)
    {
      result = msgdomain_read_tcl (tcl_locale_name, tcl_base_directory);
    }
  else
    {
      message_list_ty *mlp;

      mlp = message_list_alloc (false);
      if (optind < argc)
	{
	  do
	    read_mo_file (mlp, argv[optind]);
	  while (++optind < argc);
	}
      else
	read_mo_file (mlp, "-");

      result = msgdomain_list_alloc (false);
      result->item[0]->messages = mlp;
    }

  /* Sorting the list of messages.  */
  if (sort_by_msgid)
    msgdomain_list_sort_by_msgid (result);

  /* Write the resulting message list to the given .po file.  */
  msgdomain_list_print (result, output_file, force_po, false);

  /* No problems.  */
  exit (EXIT_SUCCESS);
}


/* Display usage information and exit.  */
static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      printf (_("\
Usage: %s [OPTION] [FILE]...\n\
"), program_name);
      printf ("\n");
      printf (_("\
Convert binary message catalog to Uniforum style .po file.\n\
"));
      printf ("\n");
      printf (_("\
Mandatory arguments to long options are mandatory for short options too.\n"));
      printf ("\n");
      printf (_("\
Operation mode:\n"));
      printf (_("\
  -j, --java                  Java mode: input is a Java ResourceBundle class\n"));
      printf (_("\
      --tcl                   Tcl mode: input is a tcl/msgcat .msg file\n"));
      printf ("\n");
      printf (_("\
Input file location:\n"));
      printf (_("\
  FILE ...                    input .mo files\n"));
      printf (_("\
If no input file is given or if it is -, standard input is read.\n"));
      printf ("\n");
      printf (_("\
Input file location in Java mode:\n"));
      printf (_("\
  -r, --resource=RESOURCE     resource name\n"));
      printf (_("\
  -l, --locale=LOCALE         locale name, either language or language_COUNTRY\n"));
      printf (_("\
The class name is determined by appending the locale name to the resource name,\n\
separated with an underscore.  The class is located using the CLASSPATH.\n\
"));
      printf ("\n");
      printf (_("\
Input file location in Tcl mode:\n"));
      printf (_("\
  -l, --locale=LOCALE         locale name, either language or language_COUNTRY\n"));
      printf (_("\
  -d DIRECTORY                base directory of .msg message catalogs\n"));
      printf (_("\
The -l and -d options are mandatory.  The .msg file is located in the\n\
specified directory.\n"));
      printf ("\n");
      printf (_("\
Output file location:\n"));
      printf (_("\
  -o, --output-file=FILE      write output to specified file\n"));
      printf (_("\
The results are written to standard output if no output file is specified\n\
or if it is -.\n"));
      printf ("\n");
      printf (_("\
Output details:\n"));
      printf (_("\
  -e, --no-escape             do not use C escapes in output (default)\n"));
      printf (_("\
  -E, --escape                use C escapes in output, no extended chars\n"));
      printf (_("\
      --force-po              write PO file even if empty\n"));
      printf (_("\
  -i, --indent                write indented output style\n"));
      printf (_("\
      --strict                write strict uniforum style\n"));
      printf (_("\
  -p, --properties-output     write out a Java .properties file\n"));
      printf (_("\
      --stringtable-output    write out a NeXTstep/GNUstep .strings file\n"));
      printf (_("\
  -w, --width=NUMBER          set output page width\n"));
      printf (_("\
      --no-wrap               do not break long message lines, longer than\n\
                              the output page width, into several lines\n"));
      printf (_("\
  -s, --sort-output           generate sorted output\n"));
      printf ("\n");
      printf (_("\
Informative output:\n"));
      printf (_("\
  -h, --help                  display this help and exit\n"));
      printf (_("\
  -V, --version               output version information and exit\n"));
      printf (_("\
  -v, --verbose               increase verbosity level\n"));
      printf ("\n");
      fputs (_("Report bugs to <bug-gnu-gettext@gnu.org>.\n"),
	     stdout);
    }

  exit (status);
}
