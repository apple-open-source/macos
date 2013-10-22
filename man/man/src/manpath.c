/*
 * manpath.c
 *
 * Copyright (c) 1990, 1991, John W. Eaton.
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with the man
 * distribution.  
 *
 * John W. Eaton
 * jwe@che.utexas.edu
 * Department of Chemical Engineering
 * The University of Texas at Austin
 * Austin, Texas  78712
 *
 * Changed PATH->manpath algorithm
 * Added: an empty string in MANPATH denotes the system path
 * Added: use LANG to search in /usr/man/<locale>
 * Lots of other minor things, including spoiling the indentation.
 * aeb - 940315
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <stdbool.h>
#include <xcselect.h>
#endif /* __APPLE__ */

/* not always in <string.h> */
extern char *index(const char *, int);
extern char *rindex(const char *, int);

#include "defs.h"
#include "gripes.h"
#include "man.h"		/* for debug */
#include "man-config.h"		/* for cfdirlist */
#include "man-getopt.h"		/* for alt_system, opt_manpath */
#include "manpath.h"
#include "util.h"		/* my_malloc, my_strdup */

char **mandirlist;
static int mandirlistlth = 0;
static int mandirlistmax = 0;

/*
 * Input: a string, with : as separator
 * For each entry in the string, call fn.
 */
static void
split (char *string, void (*fn)(char *, int), int perrs) {
     char *p, *q, *r;

     if (string) {
          p = my_strdup(string);
	  for (q = p; ; ) {
	       r = index(q, ':');
	       if (r) {
		    *r = 0;
		    fn (q, perrs);
		    q = r+1;
	       } else {
		    fn (q, perrs);
		    break;
	       }
	  }
	  free (p);
     }
}

static void
split2 (char *s, char *string, void (*fn)(char *, char *, int), int perrs) {
     char *p, *q, *r;

     if (string) {
          p = my_strdup(string);
	  for (q = p; ; ) {
	       r = index(q, ':');
	       if (r) {
		    *r = 0;
		    fn (s, q, perrs);
		    q = r+1;
	       } else {
		    fn (s, q, perrs);
		    break;
	       }
	  }
	  free (p);
     }
}

/*
 * Is path a directory?
 * -1: error, 0: no, 1: yes.
 */
static int
is_directory (char *path) {
     struct stat sb;

     if (stat (path, &sb) != 0)
	  return -1;

     return ((sb.st_mode & S_IFDIR) == S_IFDIR);
}

/*
 * Check to see if the current directory has man or MAN
 * or ../man or ../man1 or ../man8 subdirectories. 
 */
static char *
find_man_subdir (char *p) {
     int len;
     char *t, *sp;

     len = strlen (p);

     t = my_malloc ((unsigned) len + 20);

     memcpy (t, p, len);
     strcpy (t + len, "/man");
  
     if (is_directory (t) == 1)
	  return t;

     strcpy (t + len, "/MAN");
  
     if (is_directory (t) == 1)
	  return t;

     /* find parent directory */
     t[len] = 0;
     if ((sp = rindex (t, '/')) != NULL) {
	  *sp = 0;
	  len = sp - t;
     } else {
	  strcpy (t + len, "/..");
	  len += 3;
     }

#if defined(__APPLE__)
     /* Try to use share/man (3545105). */
     strcpy (t + len, "/share/man");

     if (is_directory (t) == 1)
	  return t;
#endif

     /* look for the situation with packagedir/bin and packagedir/man */
     strcpy (t + len, "/man");

     if (is_directory (t) == 1)
	  return t;

     /* look for the situation with pkg/bin and pkg/man1 or pkg/man8 */
     /* (looking for all man[1-9] would probably be a waste of stats) */
     strcpy (t + len, "/man1");

     if (is_directory (t) == 1) {
	  t[len] = 0;
	  return t;
     }

     strcpy (t + len, "/man8");

     if (is_directory (t) == 1) {
	  t[len] = 0;
	  return t;
     }

     free (t);
     return NULL;
}

/*
 * Add a directory to the manpath list if it isn't already there.
 */
static void
add_to_list (char *dir, char *lang, int perrs) {
     int status;
     char cwd[BUFSIZ];
     char **dp;

     if (!lang)
	  lang = "";

     /* only add absolute paths */
     if (*dir != '/') {
	  if (!getcwd(cwd, sizeof(cwd)))
	       return; /* cwd not readable, or pathname very long */
	  if (cwd[0] != '/')
	       return; /* strange.. */
	  if (strlen(dir) + strlen(lang) + strlen(cwd) + 3 > sizeof(cwd))
	       return;
	  if (!strncmp (dir, "./", 2))
	       dir += 2;
	  while (!strncmp (dir, "../", 3)) {
	       char *p = rindex (cwd, '/');
	       if (p > cwd)
		       *p = 0;
	       else
		       cwd[1] = 0;
	       dir += 3;
	  }
	  strcat (cwd, "/");
	  strcat (cwd, dir);
	  if (*lang) {
	       strcat (cwd, "/");
	       strcat (cwd, lang);
	  }
	  dir = cwd;
     } else if (*lang) {
	  if (strlen(dir) + strlen(lang) + 2 > sizeof(cwd))
	       return;
	  strcpy (cwd, dir);
	  strcat (cwd, "/");
	  strcat (cwd, lang);
	  dir = cwd;
     }

     if (mandirlist) {
	  for (dp = mandirlist; *dp; dp++) {
	       if (!strcmp (*dp, dir))
		    return;
	  }
     }

     /*
      * Avoid trickery: no /../ in path.
      */
     if (strstr(dir, "/../"))
	  return;

     /*
      * Not found -- add it.
      */
     status = is_directory(dir);

     if (status < 0 && perrs) {
	  gripe (CANNOT_STAT, dir);
     } else if (status == 0 && perrs) {
	  gripe (IS_NO_DIR, dir);
     } else if (status == 1) {
	  if (debug)
	       gripe (ADDING_TO_MANPATH, dir);

	  if (!mandirlist || mandirlistlth+1 >= mandirlistmax) {
	       int i, ct = mandirlistmax + 100;
	       char **p = (char **) my_malloc(ct * sizeof(char *));

	       if (mandirlist) {
		    for (i=0; i<mandirlistlth; i++)
			 p[i] = mandirlist[i];
		    free(mandirlist);
	       }
	       mandirlistmax = ct;
	       mandirlist = p;
	  }
	  mandirlist[mandirlistlth++] = my_strdup (dir);
	  mandirlist[mandirlistlth] = 0;
     }
}

static void
add_to_mandirlist_x (char *dir, char *lang, int perrs) {
	add_to_list(dir, lang, perrs);
	if (lang && strlen(lang) > 5 && lang[6] == '.') {
		char lang2[6];	/* e.g. zh_CN from zh_CN.GB2312 */

		strncpy(lang2,lang,5);
		lang2[5] = 0;
		add_to_list(dir, lang2, perrs);
	}
	if (lang && strlen(lang) > 2) {
		char lang2[3];

		strncpy(lang2,lang,2);
		lang2[2] = 0;
		add_to_list(dir, lang2, perrs);
	}
}	

static void
add_to_mandirlist (char *dir, int perrs) {
	char *lang;

	if (alt_system) {
		add_to_list(dir, alt_system_name, perrs);
	} else {
		/* We cannot use "lang = setlocale(LC_MESSAGES, NULL)" or so:
		   the return value of setlocale is an opaque string. */
		/* POSIX prescribes the order: LC_ALL, LC_MESSAGES, LANG */
		if((lang = getenv("LC_ALL")) != NULL)
			split2(dir, lang, add_to_mandirlist_x, perrs);
		if((lang = getenv("LC_MESSAGES")) != NULL)
			split2(dir, lang, add_to_mandirlist_x, perrs);
		if((lang = getenv("LANG")) != NULL)
			split2(dir, lang, add_to_mandirlist_x, perrs);
		if((lang = getenv("LANGUAGE")) != NULL)
			split2(dir, lang, add_to_mandirlist_x, perrs);
		add_to_mandirlist_x(dir, 0, perrs);
	}
}

/*
 * For each directory in the user's path, see if it is one of the
 * directories listed in the man.conf file.  If so, and it is
 * not already in the manpath, add it.  If the directory is not listed
 * in the man.conf file, see if there is a subdirectory `man' or
 * `MAN'.  If so, and it is not already in the manpath, add it.
 *
 * Example:  user has <dir>/bin in his path and the directory
 * <dir>/bin/man exists -- the directory <dir>/bin/man will be added
 * to the manpath.
 * Try also <dir>/man ?and <dir>?, and, if LANG is set, <dir>/$LANG/man.
 * aeb - 940320
 */
static void
get_manpath_from_pathdir (char *dir, int perrs) {
	char *t;
	struct dirs *dlp;

	if (debug)
		gripe (PATH_DIR, dir);

	/*
	 * The directory we're working on is in the config file.
	 * If we haven't added it to the list yet, do.
	 */
	if (*dir) {
		for (dlp = cfdirlist.nxt; dlp; dlp = dlp->nxt) {
			if (!strcmp (dir, dlp->bindir)) {
				if (debug)
					gripe (IS_IN_CONFIG);
		  
				add_to_mandirlist (dlp->mandir, perrs);
				return;
			}
		}
	}
	  
    if (!noautopath) {
        /*
         * The directory we're working on isn't in the config file.
         * See if it has man or MAN subdirectories.  If so, and this
         * subdirectory hasn't been added to the list, do. (Try also
         * a few other places nearby.)
         */
        if (debug)
            gripe (IS_NOT_IN_CONFIG);
          
        t = find_man_subdir (dir);
        if (t != NULL) {
            if (debug)
                gripe (MAN_NEARBY);
              
            add_to_mandirlist (t, perrs);
            free (t);
        } else {
            if (debug)
                gripe (NO_MAN_NEARBY);
        }
    }
}

static void
add_default_manpath (int perrs) {
     struct dirs *dlp;

     if (debug)
	  gripe (ADDING_MANDIRS);

     for (dlp = cfdirlist.nxt; dlp; dlp = dlp->nxt)
	  if (dlp->mandatory)
	       add_to_mandirlist (dlp->mandir, perrs);

#ifdef __APPLE__
	xcselect_manpaths *xcp;
	const char *path;
	unsigned i, count;

	// TODO: pass something for sdkname
	xcp = xcselect_get_manpaths(NULL);
	if (xcp != NULL) {
		count = xcselect_manpaths_get_num_paths(xcp);
		for (i = 0; i < count; i++) {
			path = xcselect_manpaths_get_path(xcp, i);
			if (path != NULL) {
				add_to_mandirlist((char *)path, perrs);
			}
		}
		xcselect_manpaths_free(xcp);
	}
#endif /* __APPLE__ */
}

static void
to_mandirlist(char *s, int perrs) {
     char *path;

     if (*s) {
	  add_to_mandirlist (s, perrs);
     } else {
	  /* empty substring: insert default path */
	  if((path = getenv ("PATH")) != NULL)
	       split (path, get_manpath_from_pathdir, perrs);
	  add_default_manpath (perrs);
     }
}

void
init_manpath () {
     static int done = 0;

     if (!done) {
	  char *manp;

	  if ((manp = opt_manpath) == NULL &&
              (manp = getenv ("MANPATH")) == NULL)
	       manp = "";		/* default path */
	  split (manp, to_mandirlist, 0);
	  done = 1;
     }
}

void
prmanpath () {
     char **dp, **dp0;

     if (mandirlist) {
	  for (dp0 = dp = mandirlist; *dp; dp++) {
	       if (dp != dp0)
		    printf(":");
	       printf("%s", *dp);
	  }
     }
     printf("\n");
}
