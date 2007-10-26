/* catopen.c - aeb, 960107 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <nl_types.h>

extern char *index (const char *, int);         /* not always in <string.h> */
extern char *my_malloc(int);	/* in util.c */

#ifndef DEFAULT_NLSPATH
# if __GLIBC__ >= 2
#  define DEFAULT_NLSPATH "/usr/share/locale/%L/%N"
# else
#  define DEFAULT_NLSPATH "/usr/lib/locale/%N/%L"
# endif
#endif

static nl_catd my_catopenpath(char *name, char *path);

static				/* this source included in gripes.c */
nl_catd
my_catopen(char *name, int oflag) {
  nl_catd fd;

  fd = catopen(name, oflag);

  if (fd == (nl_catd) -1 && oflag) {
    oflag = 0;
    fd = catopen(name, oflag);
  }

  if (fd == (nl_catd) -1) {
    char *nlspath;

    /* The libc catopen fails - let us see if we can do better */
    /* The quotes below are from X/Open, XPG 1987, Vol. 3. */

    /*
     * "In catopen(), the oflag argument is reserved for future use
     * and should be set to 0."
     */

    if (oflag)
      return fd;		/* only treat the known case */

    /*
     * "If `name' contains a `/', then `name' specifies a pathname"
     */
    if (index(name, '/')) {
#ifdef __GLIBC__
      /* glibc uses a pointer type for nl_catd, not a fd */
      return fd;
#else
      /* this works under libc5 */
      return open(name, O_RDONLY);
#endif
    }

    /*
     * "If NLSPATH does not exist in the environment, or if a
     * message catalog cannot be opened in any of the paths specified
     * by NLSPATH, then an implementation defined default path is used"
     */

    nlspath = getenv("NLSPATH");
    if (nlspath)
      fd = my_catopenpath(name, nlspath);
    if (fd == (nl_catd) -1)
      fd = my_catopenpath(name, DEFAULT_NLSPATH);
  }
  return fd;
}

/*
 * XPG 1987, Vol. 3, Section 4.4 specifies the environment
 * variable NLSPATH (example:
 *     NLSPATH=/nlslib/%L/%N.cat:/nlslib/%N/%L
 * where %L stands for the value of the environment variable LANG,
 * and %N stands for the `name' argument of catopen().
 * The environ(5) page specifies more precisely that
 * LANG is of the form
 *     LANG=language[_territory[.codeset]]
 * and adds %l, %t, %c for the three parts of LANG, without the
 * _ or . and are NULL when these parts are missing.
 * It also specifies %% for a single % sign.
 * A leading colon, or a pair of adjacent colons, in NLSPATH
 * specifies the current directory. (Meant is that "" stands for "%N".)
 *
 */
static nl_catd
my_catopenpath(char *name, char *nlspath) {
  int fd;
  nl_catd cfd = (nl_catd) -1;
  char *path0, *path, *s, *file, *lang, *lang_l, *lang_t, *lang_c;
  int langsz, namesz, sz, lang_l_sz, lang_t_sz, lang_c_sz;

  namesz = strlen(name);

  lang = getenv("LANG");
  if (!lang)
    lang = "";
  langsz = strlen(lang);

  lang_l = lang;
  s = index(lang_l, '_');
  if (!s) {
    lang_t = lang_c = "";
    lang_l_sz = langsz;
    lang_t_sz = lang_c_sz = 0;
  } else {
    lang_l_sz = s - lang_l;
    lang_t = s+1;
    s = index(lang_t, '.');
    if (!s) {
      lang_c = "";
      lang_t_sz = langsz - lang_l_sz - 1;
      lang_c_sz = 0;
    } else {
      lang_t_sz = s - lang_t;
      lang_c = s+1;
      lang_c_sz = langsz - lang_l_sz - lang_t_sz - 2;
    }
  }

  /* figure out how much room we have to malloc() */
  sz = 0;
  s = nlspath;
  while(*s) {
    if(*s == '%') {
      s++;
      switch(*s) {
      case '%':
	sz++; break;
      case 'N':
	sz += namesz; break;
      case 'L':
	sz += langsz; break;
      case 'l':
	sz += lang_l_sz; break;
      case 't':
	sz += lang_t_sz; break;
      case 'c':
	sz += lang_c_sz; break;
      default:			/* unknown escape - ignore */
	break;
      }
    } else if (*s == ':' && (s == nlspath || s[-1] == ':')) {
      sz += (namesz + 1);
    } else
      sz++;
    s++;
  }

  /* expand the %L and %N escapes */
  path0 = my_malloc(sz + 1);
  path = path0;
  s = nlspath;
  while(*s) {
    if(*s == '%') {
      s++;
      switch(*s) {
      case '%':
	*path++ = '%'; break;
      case 'N':
	strncpy(path, name, namesz); path += namesz; break;
      case 'L':
	strncpy(path, lang, langsz); path += langsz; break;
      case 'l':
	strncpy(path, lang_l, lang_l_sz); path += lang_l_sz; break;
      case 't':
	strncpy(path, lang_t, lang_t_sz); path += lang_t_sz; break;
      case 'c':
	strncpy(path, lang_c, lang_c_sz); path += lang_c_sz; break;
      default:			/* unknown escape - ignore */
	break;
      }
    } else if (*s == ':' && (s == nlspath || s[-1] == ':')) {
      strncpy(path, name, namesz); path += namesz;
      *path++ = ':';
    } else
      *path++ = *s;
    s++;
  }
  *path = 0;

  path = path0;
  while(path) {
    file = path;
    s = index(path, ':');
    if (s) {
      *s = 0;
      path = s+1;
    } else
      path = 0;
    fd = open(file, O_RDONLY);
    if (fd != -1) {
      /* we found the right catalog - but we don't know the
	 type of nl_catd, so close it again and ask libc */
      close(fd);
      cfd = catopen(file, 0);
      break;
    }
  }

  free(path0);
  return cfd;
}
