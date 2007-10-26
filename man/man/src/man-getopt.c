#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "defs.h"
#include "gripes.h"
#include "man.h"
#include "man-config.h"
#include "man-getopt.h"
#include "util.h"
#include "version.h"

int alt_system;
char *alt_system_name;
char *opt_manpath;
int global_apropos = 0;

static void
print_version (void) {
     gripe (VERSION, progname, version);
}

static void
usage (void) {
     print_version();
     gripe (USAGE1, progname);

     gripe (USAGE2);		/* only for alt_systems */

     gripe (USAGE3);
     gripe (USAGE4);
     gripe (USAGE5);    /* maybe only if troff found? */
     gripe (USAGE6);

     gripe (USAGE7);		/* only for alt_systems */

     gripe (USAGE8);
     exit(1);
}

static char short_opts[] = "B:C:H:xM:P:S:acdfFhkKm:p:s:tvVwW?Lq";

#ifndef NOGETOPT
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <getopt.h>

static const struct option long_opts[] = {
    { "help",       no_argument,            NULL, 'h' },
    { "version",    no_argument,            NULL, 'v' },
    { "path",       no_argument,            NULL, 'w' },
    { "preformat",  no_argument,            NULL, 'F' },
    { NULL, 0, NULL, 0 }
};
#endif

/*
 * Read options, return count.
 */
static int
get_options_from_argvec(int argc, char **argv, char **config_file,
			char **manpath) {
     char *s;
     int c;
     int optct = 0;

#ifndef NOGETOPT
     while ((c = getopt_long (argc, argv, short_opts, long_opts, NULL)) != -1){
#else
     while ((c = getopt (argc, argv, short_opts)) != -1) {
#endif
	  switch (c) {
	  case 'C':
	       no_privileges ();
	       if (config_file)
		       *config_file = my_strdup (optarg);
	       break;
	  case'F':
	       preformat = 1;
	       break;
	  case 'M':
	       if (manpath)
		       *manpath = my_strdup (optarg);
	       break;
	  case 'P':
	       pager = my_strdup (optarg);
	       break;
	  case 'B':
	       browser = my_strdup (optarg);
	       break;
	  case 'H':
	       htmlpager = my_strdup (optarg);
	       break;
	  case 'S':
	       colon_sep_section_list = my_strdup (optarg); 
	       break;
	  case 's':
	       /* undocumented; compatibility with Sun */
	       s = colon_sep_section_list = my_strdup (optarg);
	       while (*s) {
		       if (*s == ',')
			       *s = ':';
		       s++;
	       }
	       break;
	  case 'a':
	       findall++;
	       break;
	  case 'c':
	       nocats++;
	       break;
	  case 'd':
	       debug++;
	       break;
	  case 'f':
	       if (do_troff)
		    fatal (INCOMPAT, "-f", "-t");
	       if (apropos)
		    fatal (INCOMPAT, "-f", "-k");
	       if (print_where)
		    fatal (INCOMPAT, "-f", "-w");
	       whatis++;
	       break;
	  case 'k':
	       if (do_troff)
		    fatal (INCOMPAT, "-k", "-t");
	       if (whatis)
		    fatal (INCOMPAT, "-k", "-f");
	       if (print_where)
		    fatal (INCOMPAT, "-k", "-w");
	       apropos++;
	       break;
	  case 'K':
	       global_apropos++;
	       break;
	  case 'm':
	       alt_system++;
	       alt_system_name = my_strdup (optarg);
	       break;
	       /* or:  gripe (NO_ALTERNATE); exit(1); */
	  case 'p':
	       roff_directive = my_strdup (optarg);
	       break;
	  case 't':
	       if (apropos)
		    fatal (INCOMPAT, "-t", "-k");
	       if (whatis)
		    fatal (INCOMPAT, "-t", "-f");
	       if (print_where)
		    fatal (INCOMPAT, "-t", "-w");
	       do_troff++;
	       break;
	  case 'v':
	  case 'V':
	       print_version();
	       exit(0);
	  case 'W':
	       one_per_line++;
	       /* fall through */
	  case 'w':
	       if (apropos)
		    fatal (INCOMPAT, "-w", "-k");
	       if (whatis)
		    fatal (INCOMPAT, "-w", "-f");
	       if (do_troff)
		    fatal (INCOMPAT, "-w", "-t");
	       print_where++;
	       break;
	  /* Silently ignore manpath -q and -L (3825529). */
	  case 'L':
	  case 'q':
	       if (!strncmp(progname, "manpath", 7))
		    break;
	  case 'h':
	  case '?':
	  default:
	       usage();
	       break;
	  }
	  optct++;
     }

     return optct;
}

static void
get_options_from_string(const char *s) {
	char *s0, *ss;
	int argct;
	char **argvec;
	int optindsv;

	if (!s || *s == 0)
		return;

	/* In order to avoid having a list of options in two places,
	   massage the string so that it can be fed to getopt() */

	s0 = my_strdup(s);

	/* count arguments */
	argct = 0;
	ss = s0;
	while (*ss) {
		while (*ss == ' ')
			ss++;
		if (*ss) {
			argct++;
			while (*ss && *ss != ' ')
				ss++;
		}
	}

	/* allocate argvec */
	argvec = (char **) my_malloc((argct+2)*sizeof(char *));
	argct = 0;
	argvec[argct++] = "dummy";
	ss = s0;
	while (*ss) {
		while (*ss == ' ')
			*ss++ = 0;
		if (*ss) {
			argvec[argct++] = ss;
			while (*ss && *ss != ' ')
				ss++;
		}
	}
	argvec[argct] = 0;

	optindsv = optind;
	optind = 1;
	get_options_from_argvec(argct, argvec, NULL, NULL);
	optind = optindsv;
}

static void 
mysetenv(const char *name, const char *value) {
#if defined(__sgi__) || defined(__sun__) || defined(sun)
    int len = strlen(value)+1+strlen(value)+1;
    char *str = my_malloc(len);
    sprintf(str, "%s=%s", name, value);
    putenv(str);
#else
    setenv(name, value, 1);
#endif
}

/*
 * Get options from the command line and user environment.
 * Also reads the configuration file.
 */

void
man_getopt (int argc, char **argv) {
     char *config_file = NULL;
     char *manp = NULL;
     int optct = 0;

     optct = get_options_from_argvec(argc, argv, &config_file, &manp);

     read_config_file (config_file);

     /* If no options were given and MANDEFOPTIONS is set, use that */
     if (optct == 0) {
	     const char *defopts = getval ("MANDEFOPTIONS");
	     get_options_from_string(defopts);
     }

     /* In case an explicit -P option was given, put it in the
	environment for possible use with -k or -K.
	Ignore errors (out of memory?) */

     if (pager && (global_apropos || apropos || whatis))
	     mysetenv("PAGER", pager);

     if (pager == NULL || *pager == '\0')
	  if ((pager = getenv ("MANPAGER")) == NULL)
	       if ((pager = getenv ("PAGER")) == NULL)
		    pager = getval ("PAGER");

     if (debug)
	  gripe (PAGER_IS, pager);

     /* Ditto for BROWSER and -B */
     if (browser && (global_apropos || apropos || whatis))
	 mysetenv("BROWSER", browser);

     if (browser == NULL || *browser == '\0')
	 if ((browser = getenv ("BROWSER")) == NULL)
	     browser = getval ("BROWSER");

     if (debug)
	  gripe (BROWSER_IS, browser);

     /* Ditto for HTMLHTMLPAGER and -D */
     if (htmlpager && (global_apropos || apropos || whatis))
	 mysetenv("HTMLPAGER", htmlpager);

     if (htmlpager == NULL || *htmlpager == '\0')
	 if ((htmlpager = getenv ("HTMLPAGER")) == NULL)
	     htmlpager = getval ("HTMLPAGER");

     if (debug)
	  gripe (HTMLPAGER_IS, htmlpager);

     if (do_compress && !*getval ("COMPRESS")) {
	  if (debug)
	       gripe (NO_COMPRESS);
	  do_compress = 0;
     }

     if (do_troff && !*getval ("TROFF")) {
	  gripe (NO_TROFF, configuration_file);
	  exit (1);
     }

     opt_manpath = manp;		/* do not yet expand manpath -
					   maybe it is not needed */

     if (alt_system_name == NULL || *alt_system_name == '\0')
	  if ((alt_system_name = getenv ("SYSTEM")) != NULL)
	       alt_system_name = my_strdup (alt_system_name);

}
