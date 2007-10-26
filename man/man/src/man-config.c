/*
 * man-config.c
 *
 * Read the man.conf file
 *
 * Input line types:
 *	MANBIN		/usr/bin/man
 *	MANPATH         /usr/X386/man	[/var/catman/X386]
 *	MANPATH_MAP     /usr/bin     /usr/man
 *      FHS
 *	FSSTND
 *  NOAUTOPATH
 *	NROFF           /usr/bin/groff -Tascii -mandoc
 *	BROWSER		/usr/bin/lynx
 *	HTMLPAGER	/usr/bin/lynx -dump
 *	.gz             /usr/bin/gunzip -c
 *	# Comment
 *
 * Allow globbing in MANPATH elements.
 * This is useful e.g. for having MANPATH /opt/ * /man
 * (avoid comment within comment).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "defs.h"
#include "glob.h"
#include "man-config.h"
#include "man.h"
#include "paths.h"
#include "gripes.h"
#include "util.h"

#define BUFSIZE 4096

extern char *rindex (const char *, int);	/* not always in <string.h> */

#define whitespace(x) ((x) == ' ' || (x) == '\t')

/* directories listed in config file */
struct dirs cfdirlist;          /* linked list, 1st entry unused */

static void
addval (char *buf) {
     int i, len;
     char *bp;

     for (i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
	  len = strlen (paths[i].name);
	  bp = buf + len;
	  if(!strncmp (buf, paths[i].name, len) && (!*bp || whitespace(*bp))) {
	       while(whitespace(*bp))
		    bp++;
	       paths[i].path = my_strdup(bp);
	       return;
	  }
     }
     gripe (UNRECOGNIZED_LINE, buf);
}

const char *
getval (const char *cmd) {
     int i;

     for (i = 0; i < sizeof(paths)/sizeof(paths[0]); i++)
	  if (!strcmp (cmd, paths[i].name))
	       return paths[i].path;	/* never NULL */
     gripe (GETVAL_ERROR, cmd);
     return "";				/* impossible */
}

static void
adddir (const char *bp, int mandatory) {
     int i;
     struct dirs *dlp;

     while (whitespace(*bp))
	  bp++;
     if (*bp == 0)
	  gripe (PARSE_ERROR_IN_CONFIG);

     dlp = &cfdirlist;
     while (dlp->nxt)
	  dlp = dlp->nxt;
     dlp->nxt = (struct dirs *) my_malloc (sizeof(struct dirs));
     dlp = dlp->nxt;
     dlp->mandatory = mandatory;
     dlp->nxt = 0;

     if (!mandatory) {
	  i = 0;
	  while (*bp && !whitespace(*bp)) {
	       if (i < MAXPATHLEN - 1)
		    dlp->bindir[i++] = *bp;
	       bp++;
	  }
	  dlp->bindir[i] = 0;

	  while (whitespace(*bp))
	       bp++;
     } else {
	  dlp->bindir[0] = 0;
     }

     i = 0;
     while (*bp && !whitespace(*bp)) {
	  if (i < MAXPATHLEN - 1)
	       dlp->mandir[i++] = *bp;
	  bp++;
     }
     dlp->mandir[i] = 0;

     while (whitespace(*bp))
	  bp++;
      
     i = 0;
     while (*bp && !whitespace(*bp)) {
	  if (i < MAXPATHLEN - 1)
	       dlp->catdir[i++] = *bp;
	  bp++;
     }
     dlp->catdir[i] = 0;

     if (debug) {
	  if (dlp->mandatory)
	       gripe (FOUND_MANDIR, dlp->mandir);
	  else
	       gripe (FOUND_MAP, dlp->bindir, dlp->mandir);
	  if (dlp->catdir[0])
	       gripe (FOUND_CATDIR, dlp->catdir);
     }
}

static void
addglobdir (const char *bp, int mandatory) {
	const char *dir;

	while (whitespace(*bp))
		bp++;

	dir = bp;
	if (index(dir, '*') || index(dir, '?') || index(dir, '[')) {
		char **dp = glob_filename (dir);

		if (dp && dp != (char **) -1) {
			while (*dp)
				adddir(*dp++, mandatory);
			return;
		}
	}
	adddir(dir, mandatory);
}

static struct xp {
    char *extension;		/* non-null, including initial . */
    char *expander;
    struct xp *nxt;
} uncompressors;		/* linked list, 1st entry unused */

static void
addext (char *bp) {
     char *p, csv;
     struct xp *xpp;

     xpp = &uncompressors;
     while (xpp->nxt)
	  xpp = xpp->nxt;
     xpp->nxt = (struct xp *) my_malloc (sizeof(struct xp));
     xpp = xpp->nxt;
     xpp->nxt = 0;

     p = bp;
     while(*p && !whitespace(*p))
	  p++;
     csv = *p;
     *p = 0;
     xpp->extension = my_strdup(bp);

     *p = csv;
     while(whitespace(*p))
	  p++;
     xpp->expander = my_strdup(p);
}

const char *
get_expander (const char *file) {
     struct xp *xp;
     char *extp = NULL;

     if (dohp) {
	  /* Some HP systems have both man1 and man1.Z */
	  /* For man1.Z/file.1 let extp=".Z" */
	  /* For .1 return NULL */
	  int len = strlen (dohp);
	  char *dirname_end = rindex (file, '/');
	  if (dirname_end && !strncmp (dirname_end-len, dohp, len))
	      extp = dohp;
     } else
	  extp = rindex (file, '.');
     if (extp != NULL) {
	  if (uncompressors.nxt) {
	       for (xp = uncompressors.nxt; xp; xp = xp->nxt)
	            if (!strcmp (extp, xp->extension))
		         return (xp->expander);
	  } else if (!strcmp (extp, getval("COMPRESS_EXT"))) {
	       return getval("DECOMPRESS");
	  }
     }
     return NULL;
}

const char *configuration_file = "[no configuration file]";

char *default_config_files[] = {
     CONFIG_FILE,		/* compiled-in default */
     "/etc/man.conf", "/etc/man.config",
     "/usr/lib/man.conf", "/usr/lib/man.config",
     "/usr/share/misc/man.conf", "/usr/share/misc/man.config"
};

#define SIZE(x) (sizeof(x)/sizeof((x)[0]))

void
read_config_file (const char *cf) {
     char *bp;
     char *p;
     char buf[BUFSIZE];
     FILE *config = NULL;

     if (cf) {
	  /* User explicitly specified a config file */
	  if ((config = fopen (cf, "r")) == NULL) {
	       perror (cf);
	       gripe (CONFIG_OPEN_ERROR, cf);
	       return;
	  }
     } else {
	  /* Try some things - unfortunately we cannot lookup
	     the config file to use in the config file :-). */
	  int i;

	  for(i=0; i < SIZE(default_config_files); i++) {
	       cf = default_config_files[i];
	       if ((config = fopen (cf, "r")) != NULL)
		    break;
	  }

	  if (config == NULL) {
	       gripe (CONFIG_OPEN_ERROR, CONFIG_FILE);
	       return;
	  }
     }

     if (debug)
	  fprintf(stderr, "Reading config file %s\n", cf);
     configuration_file = cf;

     while ((bp = fgets (buf, BUFSIZE, config)) != NULL) {
	  while (whitespace(*bp))
	       bp++;

	  for (p = bp; *p && *p != '#' && *p != '\n'; p++) ;
	  if (!*p) {
	       gripe (LINE_TOO_LONG);
	       gripe (BAD_CONFIG_FILE, cf);
	       return;
	  }
	  while (p > bp && whitespace(p[-1]))
	       p--;
	  *p = 0;
      
	  if (*bp == 0)
	       continue;

	  if (!strncmp ("MANPATH_MAP", bp, 11))
	       adddir (bp+11, 0);
	  else if (!strncmp ("MANPATH", bp, 7))
	       addglobdir (bp+7, 1);
	  else if(!strncmp ("MANDATORY_MANPATH", bp, 17))/* backwards compatible */
	       adddir (bp+17, 1);
	  else if (!strncmp ("FHS", bp, 3))
	       fhs = 1;
	  else if (!strncmp ("FSSTND", bp, 6))
	       fsstnd = 1;
	  else if (!strncmp ("NOAUTOPATH", bp, 10))
		  noautopath = 1;
	  else if (!strncmp ("NOCACHE", bp, 7))
		  nocache = 1;
	  else if (*bp == '.')
	       addext (bp);
	  else
	       addval (bp);
     }
}

