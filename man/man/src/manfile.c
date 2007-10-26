/*
 * manfile.c - aeb, 971231
 *
 * Used both by man and man2html - be careful with printing!
 */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "glob.h"
#include "util.h"
#include "manfile.h"
#include "gripes.h"
#include "man.h"		/* for debug */

static int standards;
static const char *((*to_cat_filename)(const char *man_filename,
				       const char *ext, int flags));

/*
 * Append the struct or chain A to the chain HEAD.
 */
static void
append(struct manpage **head, struct manpage *a) {
     struct manpage *p;

     if (a) {
	  if (*head) {
	       p = *head;
	       while(p->next)
		    p = p->next;
	       p->next = a;
	  } else
	       *head = a;
     }
}


static int
my_lth(const char *s) {
     return s ? strlen(s) : 0;
}
 
/*
 * Find the files of the form DIR/manSEC/NAME.EXT etc.
 * Use "man" for TYPE_MAN, "cat" for TYPE_SCAT, and
 * apply convert_to_cat() to the man version for TYPE_CAT.
 *
 * Some HP systems use /usr/man/man1.Z/name.1, where name.1 is
 * compressed - yuk.  We can handle this by using section 1.Z
 * instead of 1 and assuming that the man page is compressed
 * if the directory name ends in .Z.
 *
 * Some Sun systems use /usr/share/man/sman1/man.1 and
 * /usr/share/man/sman1m/mkfs.1m.
 *
 * We support HTML filenames of the following form:
 * /usr/share/man/sman1m/mkfs.1m.html, optionally followed
 * by a compression suffix.
 *
 * Returns an array with pathnames, or 0 if out-of-memory or error.
 */
static char **
glob_for_file_ext_glob (const char *dir, const char *sec,
			const char *name, const char *ext, char *hpx,
			int glob, int type) {
     char *pathname;
     const char *p;
     char **names;
     int len;
#define MANFORM	"%s/%s%s%s/%s.%s"
#define GLOB	"*"
#define LENGTHOF(s)	(sizeof(s)-1) 
/* This must be long enough to hold the format-directory name.
 * The basic type-directory names are 'cat' and 'man'; this needs to
 * allocate space for those or any others such as html or sman.
 */
#define TYPELEN	8

     len = my_lth(dir) + my_lth(sec) + my_lth(hpx) + my_lth(name) + my_lth(ext)
	 + TYPELEN
	 + LENGTHOF(".html") + LENGTHOF(MANFORM) + LENGTHOF(GLOB);

     if (debug >= 2)
	 gripe(CALLTRACE3, dir, sec, name, ext, hpx, glob, type);

     pathname = (char *) malloc(len);
     if (!pathname)
	  return 0;

     sprintf (pathname, MANFORM,
	      dir,
	      (type==TYPE_HTML) ? "html" : (type==TYPE_XML) ? "sman" : (type==TYPE_SCAT) ? "cat" : "man", 
	      sec, hpx, name, ext);
     if (type == TYPE_HTML)
	 strcat(pathname, ".html");
     if (glob)
	 strcat(pathname, GLOB);

     if (type == TYPE_CAT) {
          p = to_cat_filename(pathname, 0, standards);
          if (p) {
	       free(pathname);
          } else {
               sprintf (pathname, "%s/cat%s%s/%s.%s%s",
                        dir, sec, hpx, name, ext, glob ? GLOB : "");
	       p = pathname;
	  }
     } else
	  p = pathname;

     if (debug >=2)
	 gripe(ABOUT_TO_GLOB, p);
     names = glob_filename (p);
     if (names == (char **) -1)		/* file system error; print msg? */
	  names = 0;
     return names;
}

static char **
glob_for_file_ext (const char *dir, const char *sec,
		   const char *name, const char *ext, int type) {
     char **names, **namesglob;
     char *hpx = ((standards & DO_HP) ? ".Z" : "");

     namesglob = glob_for_file_ext_glob(dir,sec,name,ext,hpx,1,type);
     if (!namesglob && *hpx) {
	  hpx = "";
	  namesglob = glob_for_file_ext_glob(dir,sec,name,ext,hpx,1,type);
     }
     if (!namesglob)
	  return 0;
     if (*namesglob) {
	  /* we found something - try to get a more precise match */
	  names = glob_for_file_ext_glob(dir,sec,name,ext,hpx,0,type);
	  if (names && *names)
	       namesglob = names;
     }
     return namesglob;
}

/*
 * Find the files of the form DIR/manSEC/NAME.SEC etc.
 */
static char **
glob_for_file (const char *dir, const char *sec, const char *name, int type) {
     char **names;
     char shortsec[2];

     if (debug >= 2)
	 gripe(CALLTRACE2, dir, sec, name, type);

     shortsec[0] = sec[0];
     shortsec[1] = '\0';

     if (standards & DO_IRIX) {
	  /* try first without `sec' extension */
	  /* maybe this should be done only for cat pages? */
	  return glob_for_file_ext (dir, sec, name, "", type);
     }

     /* try /usr/X11R6/man/man3x/XSetFont.3x */
     names = glob_for_file_ext (dir, shortsec, name, sec, type);

     if (!names)
	  return 0;		/* out-of-memory or error */

     /* sometimes the extension is only a single digit */
     if (!*names && isdigit(sec[0]) && sec[1] != 0) {
	  char ext[2];
	  ext[0] = sec[0];
	  ext[1] = 0;
	  names = glob_for_file_ext (dir, sec, name, ext, type);
     }

     if (!names)
	  return 0;		/* out-of-memory or error */

     /* or the extension could be .man */
     if (!*names)
	  names = glob_for_file_ext (dir, sec, name, "man", type);

     if (debug >= 2) {
	 if (!names[0])
	     gripe(NO_MATCH);
         else {
	     char **np;
	     for (np = names; *np; np++)
		 gripe(GLOB_FOR_FILE, *np);
	 }
     }

     return names;
}

/*
 * Find a man page of the given NAME under the directory DIR,
 * in section SEC. Only types (man, cat, scat, html) permitted in FLAGS
 * are allowed, and priorities are in this order.
 */
static struct manpage *
manfile_from_sec_and_dir(const char *dir,
			 const char *sec, const char *name, int flags) {
     struct manpage *res = 0;
     struct manpage *p;
     char **names, **np;
     int i, type;
     int types[] = {TYPE_HTML, TYPE_MAN, TYPE_CAT, TYPE_SCAT};

     if (debug >= 2)
	 gripe(CALLTRACE1, dir, sec, name, flags);

     for (i=0; i<(sizeof(types)/sizeof(types[0])); i++) {
	  type = types[i];

	  /* If convert_to_cat() is trivial, TYPE_CAT and TYPE_SCAT
	     are the same thing. */
	  if ((type == TYPE_CAT) && (flags & TYPE_SCAT) && !standards)
	       continue;

	  if (flags & type) {
	       names = glob_for_file (dir, sec, name, type);
	       if (names) {
		    for (np = names; *np; np++) {
#if 1
			 /* Keep looking if we encounter a file
			    we can't access */
			 if (access(*np, R_OK))
			      continue;

			 if (debug >= 2)
			     gripe(FOUND_FILE, *np);
			 /* disadvantage: no error message when permissions
			    are wrong, the page just silently becomes
			    invisible */
#endif
			 p = (struct manpage *) malloc(sizeof(*p));
			 if (!p)
			      break; 	/* %% perhaps print msg, free names */
			 p->filename = *np;
			 p->type = type;
			 p->next = 0;
			 append(&res, p);
			 if (res && (flags & ONLY_ONE_PERSEC))
			      break;
		    }
		    free(names);
	       }
	  }

	  if (res)
	       return res;
     }

     return res;
}

/*
 * Find a man page of the given NAME, searching in the specified SECTION.
 * Searching is done in all directories of MANPATH.
 */
static struct manpage *
manfile_from_section(const char *name, const char *section,
		     int flags, char **manpath) {
     char **mp;
     struct manpage *res = 0;

     for (mp = manpath; *mp; mp++) {
	  append(&res, manfile_from_sec_and_dir(*mp, section, name, flags));
	  if (res && (flags & ONLY_ONE_PERSEC))
	       break;
     }
#if 0
     /* Someone wants section 1p - better not to give 1 */
     if (res == NULL && isdigit(section[0]) && section[1]) {
	  char sec[2];

	  sec[0] = section[0];
	  sec[1] = 0;
	  for (mp = manpath; *mp; mp++) {
	       append(&res, manfile_from_sec_and_dir(*mp, sec, name, flags));
	       if (res && (flags & ONLY_ONE_PERSEC))
		    break;
	  }
     }
#endif
     return res;
}

/*
 * Find a man page of the given NAME, searching in the specified
 * SECTION, or, if that is 0, in all sections in SECTIONLIST.
 * Searching is done in all directories of MANPATH.
 * If FLAGS contains the ONLY_ONE bits, only the first matching
 * page is returned; otherwise all matching pages are found.
 * Only types (man, cat, scat) permitted in FLAGS are allowed.
 */
struct manpage *
manfile(const char *name, const char *section, int flags,
        char **sectionlist, char **manpath,
	const char *((*tocat)(const char *man_filename, const char *ext,
			      int flags))) {
     char **sl;
     struct manpage *res;

     standards = (flags & (FHS | FSSTND | DO_HP | DO_IRIX));
     to_cat_filename = tocat;

     if (name && (flags & DO_WIN32)) { /* Convert : sequences to a ? */
	  char *n = my_malloc(strlen(name) + 1);
	  const char *p = name;
	  char *q = n;

	  while (*p) {
		  if (*p == ':') {
			  *q++ = '?';
			  while (*p == ':')
				  p++;
		  } else
			  *q++ = *p++;
	  }
	  *q = 0;
	  name = n;
     }

     if (!name || !manpath)	/* error msg? */
	  res = 0;
     else if (section)
	  res = manfile_from_section(name, section, flags, manpath);
     else if (sectionlist) {
	  res = 0;
	  for (sl = sectionlist; *sl; sl++) {
	       append(&res, manfile_from_section(name, *sl, flags, manpath));
	       if (res && (flags & ONLY_ONE))
		    break;
	  }
     }
     return res;
}
