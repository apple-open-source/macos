/*
 * man.c
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
 * Some manpath, compression and locale related changes - aeb - 940320
 * Some suid related changes - aeb - 941008
 * Some more fixes, Pauline Middelink & aeb, Oct 1994
 * man -K: aeb, Jul 1995
 * Split off of manfile for man2html, aeb, New Year's Eve 1997
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>		/* for chmod */
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <locale.h>

#ifndef R_OK
#define R_OK 4
#endif

extern char *index (const char *, int);		/* not always in <string.h> */
extern char *rindex (const char *, int);	/* not always in <string.h> */

#include "defs.h"
#include "gripes.h"
#include "man.h"
#include "manfile.h"
#include "manpath.h"
#include "man-config.h"
#include "man-getopt.h"
#include "man-iconv.h"
#include "to_cat.h"
#include "util.h"
#include "glob.h"
#include "different.h"
#include "man-iconv.h"

#define SIZE(x) (sizeof(x)/sizeof((x)[0]))

const char *progname;
const char *pager, *browser, *htmlpager;
char *colon_sep_section_list;
char *roff_directive;
char *dohp = 0;
int do_irix;
int do_win32;
int apropos;
int whatis;
int nocats;			/* set by -c option: do not use cat page */
				/* this means that cat pages must not be used,
				   perhaps because the user knows they are
				   old or corrupt or so */
int can_use_cache;		/* output device is a tty, width 80 */
				/* this means that the result may be written
				   in /var/cache, and may be read from there */
int findall;
int print_where;
int one_per_line;
int do_troff;
int preformat;
int debug;
int fhs;
int fsstnd;
int noautopath;
int nocache;
static int is_japanese;
static char *language;
static char **section_list;

#ifdef DO_COMPRESS
int do_compress = 1;
#else
int do_compress = 0;
#endif

#define BUFSIZE 8192

/*
 * Try to determine the line length to use.
 * Preferences: 1. MANWIDTH, 2. ioctl, 3. COLUMNS, 4. 80
 *
 * joey, 950902
 */

#include <sys/ioctl.h>

int line_length = 80;
int ll = 0;

static void
get_line_length(void){
     char *cp;
     int width;

     if (preformat) {
	  line_length = 80;
	  return;
     }
     if ((cp = getenv ("MANWIDTH")) != NULL && (width = atoi(cp)) > 0) {
	  line_length = width;
	  return;
     }
#ifdef TIOCGWINSZ
     if (isatty(0) && isatty(1)) { /* Jon Tombs */
	  struct winsize wsz;

	  if(ioctl(0, TIOCGWINSZ, &wsz))
	       perror("TIOCGWINSZ failed\n");
	  else if(wsz.ws_col) {
	       line_length = wsz.ws_col;
	       return;
	  }
     }
#endif
     if ((cp = getenv ("COLUMNS")) != NULL && (width = atoi(cp)) > 0)
	  line_length = width;
     else
	  line_length = 80;
}

static int
setll(void) {
     return
	  (!do_troff && (line_length < 66 || line_length > 80)) ?
	  line_length*9/10 : 0;
}

/* People prefer no page headings in their man screen output;
   now ".pl 0" has a bad effect on .SH etc, so we need ".pl N"
   for some large number N, like 1100i (a hundred pages). */
#define VERY_LONG_PAGE	"1100i"

static char *
setpl(void) {
     char *pl;

     /* Short-circuit bogus behavior (3828722). */
     return NULL;

     if (do_troff)
	  return NULL;
     if (preformat)
	  pl = VERY_LONG_PAGE;
     else
     if ((pl = getenv("MANPL")) == 0) {
	  if (isatty(0) && isatty(1))
	       pl = VERY_LONG_PAGE;
	  else
	       pl = "11i";		/* old troff default */
     }
     return pl;
}

/*
 * Check to see if the argument is a valid section number.  If the
 * first character of name is a numeral, or the name matches one of
 * the sections listed in section_list, we'll assume that it's a section.
 * The list of sections in config.h simply allows us to specify oddly
 * named directories like .../man3f.  Yuk. 
 */
static char *
is_section (char *name) {
     char **vs;

     /* 3Xt may be a section, but 3DBorder is a man page */
     if (isdigit (name[0]) && !isdigit (name[1]) && strlen(name) < 5)
	  return my_strdup (name);

     for (vs = section_list; *vs != NULL; vs++)
	  if (strcmp (*vs, name) == 0)
	       return my_strdup (name);

     return NULL;
}


static void
remove_file (char *file) {
     int i;

     i = unlink (file);

     if (debug) {
	  if (i)
	       perror(file);
	  else
	       gripe (UNLINKED, file);
     }
}

static void
remove_other_catfiles (const char *catfile) {
     char *pathname;
     char *t;
     char **gf;
     int offset;

     pathname = my_strdup(catfile);
     t = rindex(pathname, '.');
     if (t == NULL || strcmp(t, getval("COMPRESS_EXT")))
	  return;
     offset = t - pathname;
     strcpy(t, "*");
     gf = glob_filename (pathname);

     if (gf != (char **) -1 && gf != NULL) {
	  for ( ; *gf; gf++) {
	       /*
		* Only remove files with a known extension, like .Z
		* (otherwise we might kill a lot when called with
		* catfile = ".gz" ...)
		*/
	       if (strlen (*gf) <= offset) {
		    if (strlen (*gf) == offset)  /* uncompressed version */
			 remove_file (*gf);
		    continue;
	       }

	       if (!strcmp (*gf + offset, getval("COMPRESS_EXT")))
		    continue;

	       if (get_expander (*gf) != NULL)
		    remove_file (*gf);
	  }
     }
}

/*
 * Simply display the preformatted page.
 */
static int
display_cat_file (const char *file) {
     int found;

     if (preformat)
	  return 1;		/* nothing to do - preformat only */

     found = 0;

     if (access (file, R_OK) == 0 && different_cat_file(file)) {
	  char *command = NULL;
	  const char *expander = get_expander (file);

	  if (expander != NULL && expander[0] != 0) {
	       if (isatty(1))
		    command = my_xsprintf("%s %S | %s", expander, file, pager);
	       else
		    command = my_xsprintf("%s %S", expander, file);
	  } else {
	       if (isatty(1)) {
		    command = my_xsprintf("%s %S", pager, file);
	       } else {
		    const char *cat = getval("CAT");
		    command = my_xsprintf("%s %S", cat[0] ? cat : "cat", file);
	       }
	  }
	  found = !do_system_command (command, 0);
     }
     return found;
}

/*
 * Simply display the preformatted page.
 */
static int
display_html_file (const char *file) {
     int found;

     found = 0;

     if (access (file, R_OK) == 0 && different_cat_file(file)) {
	  char *command = NULL;

	  if (isatty(1)) {
	       command = my_xsprintf("%s %S", browser, file);
	  } else {
	       command = my_xsprintf("%s %S", htmlpager, file);
	  }
	  found = !do_system_command (command, 0);
     }
     return found;

     return 1;
}

/*
 * Try to find the ultimate source file.  If the first line of the
 * current file is not of the form
 *
 *      .so man3/printf.3s
 *
 * the input file name is returned.
 *
 * For /cd/usr/src/usr.bin/util-linux-1.5/mount/umount.8.gz
 * (which contains `.so man8/mount.8')
 * we return /cd/usr/src/usr.bin/util-linux-1.5/mount/mount.8.gz .
 *
 * For /usr/man/man3/TIFFScanlineSize.3t
 * (which contains `.so TIFFsize.3t')
 * we return /usr/man/man3/TIFFsize.3t .
 */
static const char *
ultimate_source (const char *name0) {
     FILE *fp;
     char *name;
     const char *expander;
     int expfl = 0;
     char *fgr;
     char *beg;
     char *end;
     char *cp;
     char buf[BUFSIZE];
     static char ultname[BUFSIZE];

     if (strlen(name0) >= sizeof(ultname))
	     return name0;
     strcpy(ultname, name0);
     name = ultname;

again:
     expander = get_expander (name);
     if (expander && *expander) {
	  char *command;

	  command = my_xsprintf ("%s '%Q'", expander, name);
	  fp = my_popen (command, "r");
	  if (fp == NULL) {
	       perror("popen");
	       gripe (EXPANSION_FAILED, command);
	       return (NULL);
	  }
	  fgr = fgets (buf, sizeof(buf), fp);
	  pclose (fp);
	  expfl = 1;
     } else {
	  fp = fopen (name, "r");
	  if (fp == NULL && expfl) {
	       char *extp = rindex (name0, '.');
	       if (extp && *extp && strlen(name)+strlen(extp) < BUFSIZE) {
		    strcat(name, extp);
		    fp = fopen (name, "r");
	       }
	  }
	  /*
	   * Some people have compressed man pages, but uncompressed
	   * .so files - we could glob for all possible extensions,
	   * for now: only try .gz
	   */
	  else if (fp == NULL && get_expander(".gz") &&
		   strlen(name)+strlen(".gz") < BUFSIZE) {
	       strcat(name, ".gz");
	       fp = fopen (name, "r");
	  }

	  if (fp == NULL) {
	       perror("fopen");
	       gripe (OPEN_ERROR, name);
	       return (NULL);
	  }
	  fgr = fgets (buf, sizeof(buf), fp);
	  fclose (fp);
     }

     if (fgr == NULL) {
	  perror("fgets");
	  gripe (READ_ERROR, name);
	  return (NULL);
     }

     if (!isascii(buf[0]))
	  return (NULL);

     if (strncmp(buf, ".so", 3))
	  return (my_strdup(name));

     beg = buf+3;
     while (*beg == ' ' || *beg == '\t')
	  beg++;

     end = beg;
     while (*end != ' ' && *end != '\t' && *end != '\n' && *end != '\0')
	  end++;		/* note that buf is NUL-terminated */
     *end = '\0';

     /* If name ends in path/manx/foo.9x then use path, otherwise
	try same directory. */
     if ((cp = rindex(name, '/')) == NULL) /* very strange ... */
	  return 0;
     *cp = 0;

     /* allow "man ./foo.3" where foo.3 contains ".so man2/bar.2" */
     if ((cp = rindex(name, '/')) != NULL && !strcmp(cp+1, "."))
	  *cp = 0;

     /* In all cases, the new name will be something from name
	followed by something from beg. */
     if (strlen(name) + strlen(beg) + 1 >= BUFSIZ)
	  return 0;		/* very long names, ignore */

     if (beg[0] == '/') {
	  strcpy(name, beg);
     } else
     if (!index(beg, '/')) {
	  /* strange.. try same directory as the .so file */
	  strcat(name, "/");
	  strcat(name, beg);
     } else if((cp = rindex(name, '/')) != NULL && !strncmp(cp+1, "man", 3)) {
	  strcpy(cp+1, beg);
     } else if((cp = rindex(beg, '/')) != NULL) {
	  strcat(name, cp);
     } else {
	  strcat(name, "/");
	  strcat(name, beg);
     }

     goto again;
}

static void
add_directive (const char *d, const char *file, char *buf, int buflen) {
     if ((d = getval(d)) != 0 && *d) {
	  if (*buf == 0) {
	       if (strlen(d) + strlen(file) + 2 + 2 > buflen) // 2 extra for the single quotes
		    return;
	       strcpy (buf, d);
	       strcat (buf, " ");
	       char *fileq = my_xsprintf("'%Q'", file);
	       strcat (buf, fileq);
	       free(fileq);
	  } else {
	       if (strlen(d) + strlen(buf) + 4 > buflen)
		    return;
	       strcat (buf, " | ");
	       strcat (buf, d);
	  }
     }
}

static int
is_lang_page (char *lang, const char *file) {
	char lang_path[16] = "";

	snprintf(lang_path, sizeof(lang_path), "/%s/", lang);
	if (strstr(file, lang_path))
		return 1;
	if (strlen(lang) > 2) {
		lang_path[3] = '/';
		lang_path[4] = 0;
		if (strstr(file, lang_path))
			return 1;
	}
	return 0;
}

static int
parse_roff_directive (char *cp, const char *file, char *buf, int buflen) {
     char c;
     int tbl_found = 0;
     int use_jroff;

     use_jroff = (is_japanese &&
		   (strstr(file, "/jman/") || is_lang_page(language, file)));

     while ((c = *cp++) != '\0') {
	  switch (c) {
	  case 'e':
	       if (debug)
		    gripe (FOUND_EQN);
	       add_directive((do_troff ? "EQN" : use_jroff ? "JNEQN": "NEQN"),
			     file, buf, buflen);
	       break;

	  case 'g':
	       if (debug)
		    gripe (FOUND_GRAP);
	       add_directive ("GRAP", file, buf, buflen);
	       break;

	  case 'p':
	       if (debug)
		    gripe (FOUND_PIC);
	       add_directive ("PIC", file, buf, buflen);
	       break;

	  case 't':
	       if (debug)
		    gripe (FOUND_TBL);
	       tbl_found++;
	       add_directive ("TBL", file, buf, buflen);
	       break;

	  case 'v':
	       if (debug)
		    gripe (FOUND_VGRIND);
	       add_directive ("VGRIND", file, buf, buflen);
	       break;

	  case 'r':
	       if (debug)
		    gripe (FOUND_REFER);
	       add_directive ("REFER", file, buf, buflen);
	       break;

	  case ' ':
	  case '\t':
	  case '\n':
	       goto done;

	  default:
	       return -1;
	  }
     }

done:
     if (*buf == 0)
	  return 1;

     add_directive (do_troff ? "TROFF" : use_jroff ? "JNROFF" : "NROFF",
		    "", buf, buflen);

     if (tbl_found && !do_troff && *getval("COL"))
	  add_directive ("COL", "", buf, buflen);

     return 0;
}

static char *
eos(char *s) {
     while(*s) s++;
     return s;
}

/*
 * Create command to format FILE, in the directory PATH/manX
 */
static char *
make_roff_command (const char *path, const char *file) {
     FILE *fp;
     static char buf [BUFSIZE];
     char line [BUFSIZE], bufh [BUFSIZE], buft [BUFSIZE];
     int status, ll;
     char *cp, *fgr, *pl;
     char *command = "";
     const char *expander;
     const char *converter;

     /* if window size differs much from 80, try to adapt */
     /* (but write only standard formatted files to the cat directory,
	see can_use_cache) */
     ll = setll();
     pl = setpl();
     if (ll && debug)
	  gripe (NO_CAT_FOR_NONSTD_LL);

     expander = get_expander (file);
     converter = get_converter (path);

     /* head */
     bufh[0] = 0;
     if (ll || pl) {
	  /* some versions of echo do not accept the -e flag,
	     so we just use two echo calls when needed */
	  strcat(bufh, "(");
	  if (ll) {
	       /*
		* We should set line length and title line length.
		* However, a .lt command here fails, only
		*  .ev 1; .lt ...; .ev helps for my version of groff.
		* The LL assignment is needed by the mandoc macros.
		*/
	       sprintf(eos(bufh), "echo \".ll %d.%di\"; ", ll/10, ll%10);
	       sprintf(eos(bufh), "echo \".nr LL %d.%di\"; ", ll/10, ll%10);
#if 0
	       sprintf(eos(bufh), "echo \".lt %d.%di\"; ", ll/10, ll%10);
#endif
	  }
	  if (pl)
	       sprintf(eos(bufh), "echo \".pl %.128s\"; ", pl);
     }

     /* tail */
     buft[0] = 0;
     if (ll || pl) {
	  if (pl && !strcmp(pl, VERY_LONG_PAGE))
	      /* At end of the nroff source, set the page length to
		 the current position plus 10 lines.  This plus setpl()
		 gives us a single page that just contains the whole
		 man page. (William Webber, wew@cs.rmit.edu.au) */
	      strcat(buft, "; echo \".\\\\\\\"\"; echo \".pl \\n(nlu+10\"");
#if 0
	      /* In case this doesnt work for some reason,
		 michaelkjohnson suggests: I've got a simple
		 awk invocation that I throw into the pipeline: */

		 awk 'BEGIN {RS="\n\n\n\n*"} /.*/ {print}'
#endif
	  strcat(buft, ")");
     }

     if (expander && *expander) {
	  if (converter && *converter)
	     command = my_xsprintf("%s%s '%Q' | %s%s",
				   bufh, expander, file, converter, buft);
	  else
	     command = my_xsprintf("%s%s '%Q'%s",
				   bufh, expander, file, buft);
     } else if (ll || pl) {
	  const char *cat = getval("CAT");
	  if (!cat || !*cat)
		  cat = "cat";

	  if (converter && *converter)
	      command = my_xsprintf("%s%s '%Q' | %s%s",
				    bufh, cat, file, converter, buft);
	  else
	      command = my_xsprintf("%s%s '%Q'%s",
				    bufh, cat, file, buft);
     }

     if (strlen(command) >= sizeof(buf))
	  exit(1);
     strcpy(buf, command);

     if (roff_directive != NULL) {
	  if (debug)
	       gripe (ROFF_FROM_COMMAND_LINE);

	  status = parse_roff_directive (roff_directive, file,
					 buf, sizeof(buf));

	  if (status == 0)
	       return buf;

	  if (status == -1)
	       gripe (ROFF_CMD_FROM_COMMANDLINE_ERROR);
     }

     if (expander && *expander) {
	  char *cmd = my_xsprintf ("%s '%Q'", expander, file);
	  fp = my_popen (cmd, "r");
	  if (fp == NULL) {
	       perror("popen");
	       gripe (EXPANSION_FAILED, cmd);
	       return (NULL);
	  }
	  fgr = fgets (line, sizeof(line), fp);
	  pclose (fp);
     } else {
	  fp = fopen (file, "r");
	  if (fp == NULL) {
	       perror("fopen");
	       gripe (OPEN_ERROR, file);
	       return (NULL);
	  }
	  fgr = fgets (line, sizeof(line), fp);
	  fclose (fp);
     }

     if (fgr == NULL) {
	  perror("fgets");
	  gripe (READ_ERROR, file);
	  return (NULL);
     }

     cp = &line[0];
     if (*cp++ == '\'' && *cp++ == '\\' && *cp++ == '"' && *cp++ == ' ') {
	  if (debug)
	       gripe (ROFF_FROM_FILE, file);

	  status = parse_roff_directive (cp, file, buf, sizeof(buf));

	  if (status == 0)
	       return buf;

	  if (status == -1)
	       gripe (ROFF_CMD_FROM_FILE_ERROR, file);
     }

     if ((cp = getenv ("MANROFFSEQ")) != NULL) {
	  if (debug)
	       gripe (ROFF_FROM_ENV);

	  status = parse_roff_directive (cp, file, buf, sizeof(buf));

	  if (status == 0)
	       return buf;

	  if (status == -1)
	       gripe (MANROFFSEQ_ERROR);
     }

     if (debug)
	  gripe (USING_DEFAULT);

     (void) parse_roff_directive ("t", file, buf, sizeof(buf));

     return buf;
}

/*
 * Try to format the man page and create a new formatted file.  Return
 * 1 for success and 0 for failure.
 */
static int
make_cat_file (const char *path, const char *man_file, const char *cat_file) {
     int mode;
     FILE *fp;
     char *roff_command;
     char *command = NULL;
     struct stat statbuf;

     /* _Before_ first, make sure we will write to a regular file. */
     if (stat(cat_file, &statbuf) == 0) {
	  if(!S_ISREG(statbuf.st_mode)) {
	       if (debug)
		    gripe (CAT_OPEN_ERROR, cat_file);
	       return 0;
	  }
     }

     /* First make sure we can write the file; create an empty file. */
     /* If we are suid it must get mode 0666. */
     if ((fp = fopen (cat_file, "w")) == NULL) {
	  if (errno == ENOENT)		/* directory does not exist */
	       return 0;

	  /* If we cannot write the file, maybe we can delete it */
	  if(unlink (cat_file) != 0 || (fp = fopen (cat_file, "w")) == NULL) {
	       if (errno == EROFS) 	/* possibly a CDROM */
		    return 0;
	       if (debug)
		    gripe (CAT_OPEN_ERROR, cat_file);
	       if (!suid)
		    return 0;

	       /* maybe the real user can write it */
	       /* note: just doing "> %s" gives the wrong exit status */
	       command = my_xsprintf("cp /dev/null %S 2>/dev/null", cat_file);
	       if (do_system_command(command, 1)) {
		    if (debug)
			 gripe (USER_CANNOT_OPEN_CAT);
		    return 0;
	       }
	       if (debug)
		    gripe (USER_CAN_OPEN_CAT);
	  }
     } else {
	  /* we can write it - good */
	  fclose (fp);

	  /* but maybe the real user cannot - let's allow everybody */
	  /* the mode is reset below */
	  if (suid) {
	       if (chmod (cat_file, 0666)) {
		    /* probably we are sgid but not owner;
		       just delete the file and create it again */
		    if(unlink(cat_file) != 0) {
			 command = my_xsprintf("rm %S", cat_file);
			 (void) do_system_command (command, 1);
		    }
		    if ((fp = fopen (cat_file, "w")) != NULL)
			 fclose (fp);
	       }
          }
     }

     roff_command = make_roff_command (path, man_file);
     if (roff_command == NULL)
	  return 0;
     if (do_compress)
	  /* The cd is necessary, because of .so commands,
	     like .so man1/bash.1 in bash_builtins.1.
	     But it changes the meaning of man_file and cat_file,
	     if these are not absolute. */
	
	  command = my_xsprintf("(cd %S && %s | %S > %S)", path,
		   roff_command, getval("COMPRESS"), cat_file);
     else
	  command = my_xsprintf ("(cd %S && %s > %S)", path,
		   roff_command, cat_file);

     /*
      * Don't let the user interrupt the system () call and screw up
      * the formatted man page if we're not done yet.
      */
     signal (SIGINT, SIG_IGN);

     gripe (PLEASE_WAIT);

     if (!do_system_command (command, 0)) {
	  /* success */
	  mode = ((ruid != euid) ? 0644 : (rgid != egid) ? 0464 : 0444);
	  if(chmod (cat_file, mode) != 0 && suid) {
	       command = my_xsprintf ("chmod 0%o %S", mode, cat_file);
	       (void) do_system_command (command, 1);
	  }
	  /* be silent about the success of chmod - it is not important */
	  if (debug)
	       gripe (CHANGED_MODE, cat_file, mode);
     } else {
	  /* something went wrong - remove garbage */
	  if(unlink(cat_file) != 0 && suid) {
	       command = my_xsprintf ("rm %S", cat_file);
	       (void) do_system_command (command, 1);
	  }
     }

     signal (SIGINT, SIG_DFL);

     return 1;
}

static int
display_man_file(const char *path, const char *man_file) {
     char *roff_command;
     char *command;

     if (!different_man_file (man_file))
	  return 0;
     roff_command = make_roff_command (path, man_file);
     if (roff_command == NULL)
	  return 0;
     if (do_troff)
	  command = my_xsprintf ("(cd '%Q' && %s)", path, roff_command);
     else
	  command = my_xsprintf ("(cd '%Q' && %s | (%s || true))", path,
		   roff_command, pager);

     return !do_system_command (command, 0);
}

/*
 * make and display the cat file - return 0 if something went wrong
 */
static int
make_and_display_cat_file (const char *path, const char *man_file) {
     const char *cat_file;
     const char *ext;
     int status;
     int standards;

     ext = (do_compress ? getval("COMPRESS_EXT") : 0);

     standards = (fhs ? FHS : 0) | (fsstnd ? FSSTND : 0) | (dohp ? DO_HP : 0);

     if ((cat_file = convert_to_cat(man_file, ext, standards)) == NULL)
	  return 0;

     if (debug)
	  gripe (PROPOSED_CATFILE, cat_file);

     /*
      * If cat_file exists, check whether it is more recent.
      * Otherwise, check for other cat files (maybe there are
      * old .Z files that should be removed).
      */

     status = ((nocats | preformat) ? -2 : is_newer (man_file, cat_file));
     if (debug)
	  gripe (IS_NEWER_RESULT, status);
     if (status == -1 || status == -3) {
	  /* what? man_file does not exist anymore? */
	  gripe (CANNOT_STAT, man_file);
	  return(0);
     }

     if (status != 0 || access (cat_file, R_OK) != 0) {
	  /*
	   * Cat file is out of date (status = 1) or does not exist or is
	   * empty or is to be rewritten (status = -2) or is unreadable.
	   * Try to format and save it.
	   */
	  if (print_where) {
	       printf ("%s\n", man_file);
	       return 1;
	  }

	  if (!make_cat_file (path, man_file, cat_file))
	       return 0;

	  /*
	   * If we just created this cat file, unlink any others.
	   */
	  if (status == -2 && do_compress)
	       remove_other_catfiles(cat_file);
     } else {
	  /*
	   * Formatting not necessary.  Cat file is newer than source
	   * file, or source file is not present but cat file is.
	   */
	  if (print_where) {
	       if (one_per_line) {
		    /* addition by marty leisner - leisner@sdsp.mc.xerox.com */
		    printf("%s\n", cat_file);
		    printf("%s\n", man_file);
	       } else
		    printf ("%s (<-- %s)\n", cat_file, man_file);
	       return 1;
	  }
     }
     (void) display_cat_file (cat_file);
     return 1;
}

/*
 * Try to format the man page source and save it, then display it.  If
 * that's not possible, try to format the man page source and display
 * it directly.
 */
static int
format_and_display (const char *man_file) {
     const char *path;

     if (access (man_file, R_OK) != 0)
	  return 0;

     path = mandir_of(man_file);
     if (path == NULL)
	  return 0;

     /* first test for contents  .so man1/xyzzy.1  */
     /* (in that case we do not want to make a cat file identical
	to cat1/xyzzy.1) */
     man_file = ultimate_source (man_file);
     if (man_file == NULL)
	  return 0;

     if (do_troff) {
	  char *command;
	  char *roff_command = make_roff_command (path, man_file);

	  if (roff_command == NULL)
	       return 0;

	  command = my_xsprintf("(cd %S && %s)", path, roff_command);
	  return !do_system_command (command, 0);
     }

     if (can_use_cache && make_and_display_cat_file (path, man_file))
	  return 1;

     /* line length was wrong or could not display cat_file */
     if (print_where) {
	  printf ("%s\n", man_file);
	  return 1;
     }

     return display_man_file (path, man_file);
}

/*
 * Search for manual pages.
 *
 * If preformatted manual pages are supported, look for the formatted
 * file first, then the man page source file.  If they both exist and
 * the man page source file is newer, or only the source file exists,
 * try to reformat it and write the results in the cat directory.  If
 * it is not possible to write the cat file, simply format and display
 * the man file.
 *
 * If preformatted pages are not supported, or the troff option is
 * being used, only look for the man page source file.
 *
 * Note that globbing is necessary also if the section is given,
 * since a preformatted man page might be compressed.
 *
 */
static int
man (const char *name, const char *section) {
     int found, type, flags;
     struct manpage *mp;

     found = 0;

     /* allow  man ./manpage  for formatting explicitly given man pages */
     if (index(name, '/')) {
	  char fullname[BUFSIZE];
	  char fullpath[BUFSIZE];
	  char *path;
	  char *cp;
	  FILE *fp = fopen(name, "r");

	  if (!fp) {
	       perror(name);
	       return 0;
	  }
	  fclose (fp);
	  if (*name != '/' && getcwd(fullname, sizeof(fullname))
	      && strlen(fullname) + strlen(name) + 3 < sizeof(fullname)) {
	       strcat (fullname, "/");
	       strcat (fullname, name);
	  } else if (strlen(name) + 2 < sizeof(fullname)) {
	       strcpy (fullname, name);
	  } else {
	       fprintf(stderr, "%s: name too long\n", name);
	       return 0;
	  }

	  strcpy (fullpath, fullname);
	  if ((cp = rindex(fullpath, '/')) != NULL
	      && cp-fullpath+4 < sizeof(fullpath)) {
	       strcpy(cp+1, "..");
	       path = fullpath;
	  } else
	       path = ".";

	  name = ultimate_source (fullname);
	  if (!name)
	       return 0;

	  if (print_where) {
	       printf("%s\n", name);
	       return 1;
	  }
	  return display_man_file (path, name);
     }

     fflush (stdout);
     init_manpath();

     can_use_cache = nocache ? 0 : (preformat || print_where ||
		      (isatty(0) && isatty(1) && !setll()));

     if (do_troff) {
	  const char *t = getval("TROFF");
	  if (!t || !*t)
	       return 0;	/* don't know how to format */
	  type = TYPE_MAN;
     } else {
	  const char *n = getval("NROFF");
	  type = 0;
	  if (can_use_cache)
	       type |= TYPE_CAT;
	  if (n && *n)
	       type |= TYPE_MAN;
	  if (fhs || fsstnd)
	       type |= TYPE_SCAT;

	  n = getval("BROWSER");
	  if (n && *n)
	      type |= TYPE_HTML;
     }

     flags = type;
     if (!findall)
	  flags |= ONLY_ONE;
     if (fsstnd)
	  flags |= FSSTND;
     else if (fhs)
	  flags |= FHS;
     if (dohp)
	  flags |= DO_HP;
     if (do_irix)
	  flags |= DO_IRIX;
     if (do_win32)
	  flags |= DO_WIN32;

     mp = manfile(name, section, flags, section_list, mandirlist,
		  convert_to_cat);
     found = 0;
     while (mp) {
          if (mp->type == TYPE_MAN) {
	       found = format_and_display(mp->filename);
	  } else if (mp->type == TYPE_CAT || mp->type == TYPE_SCAT) {
               if (print_where) {
                    printf ("%s\n", mp->filename);
                    found = 1;
               } else
	            found = display_cat_file(mp->filename);
	  } else if (mp->type == TYPE_HTML) {
               if (print_where) {
                    printf ("%s\n", mp->filename);
                    found = 1;
               } else
	            found = display_html_file(mp->filename);
	  } else
	       /* internal error */
	       break;
	  if (found && !findall)
	       break;
	  mp = mp->next;
     }
     return found;
}

static char **
get_section_list (void) {
     int i;
     const char *p;
     char *end;
     static char *tmp_section_list[100];

     if (colon_sep_section_list == NULL) {
	  if ((p = getenv ("MANSECT")) == NULL)
	       p = getval ("MANSECT");
	  colon_sep_section_list = my_strdup (p);
     }

     i = 0;
     for (p = colon_sep_section_list; ; p = end+1) {
	  if ((end = strchr (p, ':')) != NULL)
	       *end = '\0';

	  tmp_section_list[i++] = my_strdup (p);

	  if (end == NULL || i+1 == SIZE(tmp_section_list))
	       break;
     }

     tmp_section_list [i] = NULL;
     return tmp_section_list;
}

/* return 0 when all was OK */
static int
do_global_apropos (char *name, char *section) {
     char **dp, **gf;
     char *pathname;
     char *command;
     int status, res;

     status = 0;
     init_manpath();
     if (mandirlist)
	for (dp = mandirlist; *dp; dp++) {
	  if (debug)
	       gripe(SEARCHING, *dp);
	  pathname = my_xsprintf("%s/man%s/*", *dp, section ? section : "*");
	  gf = glob_filename (pathname);
	  free(pathname);

	  if (gf != (char **) -1 && gf != NULL) {
	       for( ; *gf; gf++) {
		    const char *expander = get_expander (*gf);
		    if (expander)
			 command = my_xsprintf("%s %S | grep '%Q'"
					       "> /dev/null 2> /dev/null",
				 expander, *gf, name);
		    else
			 command = my_xsprintf("grep '%Q' %S"
					       "> /dev/null 2> /dev/null",
				 name, *gf);
		    res = do_system_command (command, 1);
		    status |= res;
		    free (command);
		    if (res == 0) {
			 if (print_where)
			      printf("%s\n", *gf);
			 else {
			      /* should read LOCALE, but libc 4.6.27 doesn't
				 seem to handle LC_RESPONSE yet */
			      int answer, c;
			      char path[BUFSIZE];

			      printf("%s? [ynq] ", *gf);
			      fflush(stdout);
			      answer = c = getchar();
			      while (c != '\n' && c != EOF)
				   c = getchar();
			      if(index("QqXx", answer))
				   exit(0);
			      if(index("YyJj", answer)) {
				   char *ri;

				   strcpy(path, *gf);
				   ri = rindex(path, '/');
				   if (ri)
					*ri = 0;
				   format_and_display(*gf);
			      }
			 }
		    }
	       }
	  }
     }
     return status;
}

/* Special code for Japanese (to pick jnroff instead of nroff, etc.) */
static void
setlang(void) {
	char *lang;

	/* We use getenv() instead of setlocale(), because of
	   glibc 2.1.x security policy for SetUID/SetGID binary. */
	if ((lang = getenv("LANG")) == NULL &&
	    (lang = getenv("LC_ALL")) == NULL &&
	    (lang = getenv("LC_CTYPE")) == NULL)
		/* nothing */;

	language = lang;
	is_japanese = (lang && strncmp(lang, "ja", 2) == 0);
}

/*
 * Handle the apropos option.  Cheat by using another program.
 */
static int
do_apropos (char *name) {
	char *command;

	command = my_xsprintf("'%s' '%Q'", getval("APROPOS"), name);
	return do_system_command (command, 0);
}

/*
 * Handle the whatis option.  Cheat by using another program.
 */
static int
do_whatis (char *name) {
	char *command;

	command = my_xsprintf("'%s' '%Q'", getval("WHATIS"), name);
	return do_system_command (command, 0);
}

int
main (int argc, char **argv) {
     int status = 1;
     char *nextarg;
     char *tmp;
     char *section = 0;

#ifdef __CYGWIN__
     extern int optind;
#endif


#if 0
     {
	/* There are no known cases of buffer overflow caused by
	   excessively long environment variables. In case you find one,
	   the simplistic way to fix is to enable this stopgap. */
	char *s;
#define CHECK(p,l) s=getenv(p); if(s && strlen(s)>(l)) { fprintf(stderr, "ERROR: Environment variable %s too long!\n", p); exit(1); }
	CHECK("LANG", 32);
	CHECK("LANGUAGE", 128);
	CHECK("LC_MESSAGES", 128);
	CHECK("MANPAGER", 128);
	CHECK("MANPL", 128);
	CHECK("MANROFFSEQ", 128);
	CHECK("MANSECT", 128);
	CHECK("MAN_HP_DIREXT", 128);
	CHECK("PAGER", 128);
	CHECK("SYSTEM", 64);
	CHECK("BROWSER", 64);
	CHECK("HTMLPAGER", 64);
	/* COLUMNS, LC_ALL, LC_CTYPE, MANPATH, MANWIDTH, MAN_IRIX_CATNAMES,
	   MAN_ICONV_PATH, MAN_ICONV_OPT, MAN_ICONV_INPUT_CHARSET,
	   MAN_ICONV_OUTPUT_CHARSET, NLSPATH, PATH */
     }
#endif


#ifndef __FreeBSD__ 
     /* Slaven Rezif: FreeBSD-2.2-SNAP does not recognize LC_MESSAGES. */
     setlocale(LC_CTYPE, "");	/* used anywhere? maybe only isdigit()? */
     setlocale(LC_MESSAGES, "");
#endif

     /* No doubt we'll need some generic language code here later.
	For the moment only Japanese support. */
     setlang();

     /* Handle /usr/man/man1.Z/name.1 nonsense from HP */
     dohp = getenv("MAN_HP_DIREXT");		/* .Z */

     /* Handle ls.z (instead of ls.1.z) cat page naming from IRIX */
     if (getenv("MAN_IRIX_CATNAMES"))
	  do_irix = 1;

     /* Handle lack of ':' in NTFS file names */
#if defined(_WIN32) || defined(__CYGWIN__)
     do_win32 = 1;
#endif

     progname = mkprogname (argv[0]);

     get_permissions ();
     get_line_length();

     /*
      * read command line options and man.conf
      */
     man_getopt (argc, argv);

     /*
      * manpath  or  man --path  or  man -w  will only print the manpath
      */
     if (!strcmp (progname, "manpath") || (optind == argc && print_where)) {
	  init_manpath();
	  prmanpath();
	  exit(0);
     }

     if (optind == argc)
	  gripe(NO_NAME_NO_SECTION);

     section_list = get_section_list ();

     while (optind < argc) {
	  nextarg = argv[optind++];

	  /* is_section correctly accepts 3Xt as section, but also 9wm,
	     so we should not believe is_section() for the last arg. */
	  tmp = is_section (nextarg);
	  if (tmp && optind < argc) {
		  section = tmp;
		  if (debug)
			  gripe (SECTION, section);
		  continue;
	  }

	  if (global_apropos)
	       status = !do_global_apropos (nextarg, section);
	  else if (apropos)
	       status = !do_apropos (nextarg);
	  else if (whatis)
	       status = !do_whatis (nextarg);
	  else {
	       status = man (nextarg, section);

	       if (status == 0) {
		    if (section)
			 gripe (NO_SUCH_ENTRY_IN_SECTION, nextarg, section);
		    else
			 gripe (NO_SUCH_ENTRY, nextarg);
	       }
	  }
     }
     return status ? EXIT_SUCCESS : EXIT_FAILURE;
}
