/* APPLE LOCAL PFE */
/* Persistent Front End (PFE) debugging and statistics options processing.
   Copyright (C) 2001
   Free Software Foundation, Inc.
   Contributed by Apple Computer Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifdef TESTING

#include <stdio.h>
#include <stdlib.h>
#define PARAMS(x) x
#define xmalloc malloc
#define xrealloc realloc
#define progname "progname"
#define N_(msgid) (msgid)
#define _(msgid) (msgid)
#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))
#define skip_leading_substring(whole,  part) \
   (strncmp (whole, part, strlen (part)) ? NULL : whole + strlen (part))
static int test1 = 0;
static int test2 = 0;

#else

#include "config.h"
#include "system.h"
#include "toplev.h"
#include "intl.h"

#include "pfe.h"
#include "pfe-header.h"

#include <string.h>
#include <ctype.h>

#endif

#include <string.h>

#define MAXARGS 50

void pfe_decode_dbgpfe_options PARAMS ((const char *));
static void build_argv PARAMS ((const char *, int *, char **argv[]));
static void display_dbgpfe_help PARAMS ((void));

/*-------------------------------------------------------------------*/

static int do_help = 0;
static int pfe_check_structs = 0;
int pfe_display_memory_stats = 0;
int pfe_display_precomp_headers = 0;
int pfe_display_tree_walk = 0;

/*-------------------------------------------------------------------*/

typedef struct {
  const char *string_opt;
  const char letter_opt; 
  int *variable;
  int on_value;
  const char *description;
} pfe_dbg_options;

static pfe_dbg_options pfe_options[] =
{
  {"help", '?', &do_help, 1,
   N_("Display this help information")},
   
 #if TESTING
   {"test1", '1', &test1, 1, N_("test1 info")},
   {"test2", '2', &test2, 1, N_("test2 info")},
#endif
   
  {"memory-stats", 'm', &pfe_display_memory_stats, 1,
   N_("Display pfe memory statistics")},
  {"header-list", 'h', &pfe_display_precomp_headers, 1,
   N_("Display precompiled header list")},
  {"tree-walk", 't', &pfe_display_tree_walk, 1,
   N_("Display tree walk information")},
  {"check-structs", 's', &pfe_check_structs, 1,
   N_("Check structs we freeze/thaw for change in size after a FSF merge")}
};

/*-------------------------------------------------------------------*/

/* Decode the private pfe debugging and statistics options from the
  compiler command line for -fpfedbg=options (or -fddbpfe=options).  */
void 
pfe_decode_dbgpfe_options (options)
     const char *options;
{
  int argc, processed, i, j, k, len, no, bad;
  char **argv, opt, bad_letters[256];
  
  build_argv (options, &argc, &argv);
   
  for (i = 1; i < argc; ++i)
    {
      processed = bad = 0;
      bad_letters[0] = '\0';
      
      no = (argv[i][0] == 'n' && argv[i][1] == 'o' && argv[i][2] == '-');
      
      /* Try for options which are controlled by pfe_options[].
         This take the form 'foo' or 'no-foo'.  */
         
      for (j = ARRAY_SIZE (pfe_options); j--;)
	{
	  if (no)
	    {
	      if (strcmp (&argv[i][3], pfe_options[j].string_opt) == 0)
		{
		  if (pfe_options[j].variable)
		    *pfe_options[j].variable = ! pfe_options[j].on_value;
		  processed = 1;
		  break;
		}
	    }
	  else
	    {
	      if (strcmp (argv[i], pfe_options[j].string_opt) == 0)
		{
		  if (pfe_options[j].variable)
		    *pfe_options[j].variable = pfe_options[j].on_value;
		  processed = 1;
		  break;
		}
	    }
	}
	
      if (processed)
	continue;
	
      /* Special case options.  These are options for which more has to
         be done than just (re)set a single switch or options which take
         an argument following an equals sign.  For example,
         
         if (strcmp (argv[i], "non-standard-option") == 0)
           ...do non-standard things for this option...
         else if (param = skip_leading_substring (argv[i], "opt-with-arg="))
           ...process "opt-with-arg"s argument now pointed to by param...
           
         Don't forget to add all these special guys to display_dbgpfe_help().  */ 
	
      if (0) /* place-holder for non-standard and special options */
        ;
      else
        {
          /* No keywords matched so see if the option is a sequence of
             single letters.  The entire sequence could have been
             prefixed with "no-" meaning to negate the setting of
             the options indicated by the letters.  */
          
          len  = strlen (argv[i]);
          
          for (k = no ? 3 : 0; k < len; ++k)
            {
              opt = argv[i][k];
              processed = 0;

	      for (j = ARRAY_SIZE (pfe_options); j--;)
		{
		  if (opt == pfe_options[j].letter_opt)
		    {
		      if (no)
		        {
			  if (pfe_options[j].variable)
			    *pfe_options[j].variable = ! pfe_options[j].on_value;
			}
		      else
		        {
			  if (pfe_options[j].variable)
			    *pfe_options[j].variable = pfe_options[j].on_value;
		      	}
		      processed = 1;
		      break;
		    }
		}
		
	       if (!processed && strchr (bad_letters, opt) == NULL)
	         {
	           bad_letters[bad++] = opt;
	           bad_letters[bad] = '\0';
	         }
	    }
        }
    
      if (bad)
	{
	  fprintf (stderr, "Invalid option %s skipped", argv[i]);
	  if (bad < (int)strlen (argv[i]) - (no ? 3 : 0))
	    {
	      fprintf (stderr, " (letter%s not recognized %s)",
		       bad != 1 ? "s" : "", bad_letters);
	    }
	  fputc ('\n', stderr);
	}
    }
      
    if (do_help)
      display_dbgpfe_help ();
      
    if (pfe_check_structs)
      pfe_check_all_struct_sizes ();
}

static void
display_dbgpfe_help ()
{
  int i;
  
  fprintf (stderr, "PFE Debugging and Statistics Options...\n");
  
  for (i = 0; i < (int)ARRAY_SIZE (pfe_options); ++i)
    {
      const char *description = pfe_options[i].description;

      if (description && *description)
	fprintf (stderr, "  %-21s %s\n", pfe_options[i].string_opt, _(description));
    }
}

/* Convert the options to a conventional argv vector.  Returns the
   vector of argc string pointers in the array pointed to by argv.  */
   
static void 
build_argv (options, argc, argv)
     const char *options;
     int *argc;
     char **argv[];
{
  char c, *cp, *ap;
  int len, in_string;
 
  static char *myArgv[MAXARGS+1];
  static char *cmdline = NULL;
  
  len = strlen (options) + 1;
  cmdline = cmdline ? xrealloc (cmdline, len) : xmalloc (len);
  strcpy (cmdline, options);
  
  *argv = myArgv;
  myArgv[(*argc = 0)] = (char *)progname;
  cp = (unsigned char *)(cmdline - 1);
  
  while (*argc < MAXARGS-1) {
    while (isspace(*++cp)) ;
    if ((c = *cp) == '\0')
      break;
    
    in_string = (*cp == '\'' || *cp == '"') ? *cp++ : 0;
    ap = cp;
    myArgv[++(*argc)] = (char *)cp;
    
    while ((c = *++cp) != '\0')
      {
	if (!in_string && isspace(c))
	  break;
	
	if (c != '\\')
	  {
	    if (!in_string)
	      {
	        if (c == '\'' || c == '"')
	          in_string = c;
	        else
	          *++ap = c;
	      }
	    else if (c != in_string)
	      *++ap = c;
	    else
	      in_string = 0;
	  }
	else if ((c = *++cp) == '\0') 
	  *++ap = *--cp;
	else
	  switch (c) 
	  {
	    case 'n': *++ap = '\n';
	    case 'r': *++ap = '\r';
	    case 't': *++ap = '\t';
	    case 'a': *++ap = '\a';
	    case 'f': *++ap = '\f';
	    case 'b': *++ap = '\b';
	    case 'v': *++ap = '\v';
	    default : *++ap = c;
	  }
      }
    
    *++ap = '\0';
    
    if (c == '\0')
      break;
  }

  myArgv[++(*argc)] = NULL;
}

/*-------------------------------------------------------------------*/

#ifdef TESTING

int main(argc, argv)
  int argc;
  char *argv[];
{
  char *s, cmdline[256];
  int i;
  
  while (1)
    {
      fprintf (stderr, "? ");
      s = fgets (cmdline, 255, stdin);
      if (!s)
        break;
      
      #if 1
      pfe_decode_dbgpfe_options (cmdline);
      #else
      build_argv (cmdline, &argc, &argv);
      
      for (i = 0; i < argc; ++i)
      	fprintf (stderr, "   argv[%d] = \"%s\"\n", i, argv[i]);
      #endif
      
      fputc ('\n', stderr);
    }
      
  return 0;
}
#endif

