/*
  Copyright (c) 1990-1999 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 1999-Oct-05 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, both of these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.cdrom.com/pub/infozip/license.html
*/
#include "zip.h"

#include <time.h>
#include <unixlib.h>

#include <descrip.h>
#include <rms.h>
#include <ssdef.h>
#include <starlet.h>

#define PATH_START '['
#define PATH_END ']'
#define PATH_START2 '<'
#define PATH_END2 '>'
#include "vms/vmsmunch.h"

/* Extra malloc() space in names for cutpath() */
#define PAD 5         /* may have to change .FOO] to ]FOO.DIR;1 */


#ifndef UTIL    /* the companion #endif is a bit of ways down ... */

/* The C RTL from OpenVMS 7.0 and newer supplies POSIX compatible versions of
 * opendir() et al. Thus, we have to use other names in our private code for
 * directory scanning to prevent symbol name conflicts at link time.
 * For now, we do not use the library supplied "dirent.h" functions, since
 * our private implementation provides some functionality which may not be
 * present in the library versions.  For example:
 * ==> zopendir("DISK:[DIR.SUB1]SUB2.DIR") scans "DISK:[DIR.SUB1.SUB2]".
 */

typedef struct zdirent {
  int d_wild;                /* flag for wildcard vs. non-wild */
  struct FAB fab;
  struct NAM nam;
  char d_qualwildname[NAM$C_MAXRSS + 1];
  char d_name[NAM$C_MAXRSS + 1];
} zDIR;

extern char *label;
local ulg label_time = 0;
local ulg label_mode = 0;
local time_t label_utim = 0;

/* Local functions */
local void vms_wild OF((char *, zDIR *));
local zDIR *zopendir OF((ZCONST char *));
local char *readd OF((zDIR *));
local char *strlower OF((char *));
local char *strupper OF((char *));


/*---------------------------------------------------------------------------

    _vms_findfirst() and _vms_findnext(), based on public-domain DECUS C
    fwild() and fnext() routines (originally written by Martin Minow, poss-
    ibly modified by Jerry Leichter for bintnxvms.c), were written by Greg
    Roelofs and are still in the public domain.  Routines approximate the
    behavior of MS-DOS (MSC and Turbo C) findfirst and findnext functions.

  ---------------------------------------------------------------------------*/

static char wild_version_part[10]="\0";

local void vms_wild(p, d)
char *p;
zDIR *d;
{
  /*
   * Do wildcard setup
   */
  /* set up the FAB and NAM blocks. */
  d->fab = cc$rms_fab;             /* initialize fab */
  d->nam = cc$rms_nam;             /* initialize nam */

  d->fab.fab$l_nam = &d->nam;           /* fab -> nam */
  d->fab.fab$l_fna = p;                 /* argument wild name */
  d->fab.fab$b_fns = strlen(p);         /* length */

  d->fab.fab$l_dna = "sys$disk:[]";     /* Default fspec */
  d->fab.fab$b_dns = sizeof("sys$disk:[]")-1;

  d->nam.nam$l_esa = d->d_qualwildname; /* qualified wild name */
  d->nam.nam$b_ess = NAM$C_MAXRSS;      /* max length */
  d->nam.nam$l_rsa = d->d_name;         /* matching file name */
  d->nam.nam$b_rss = NAM$C_MAXRSS;      /* max length */

  /* parse the file name */
  if (sys$parse(&d->fab) != RMS$_NORMAL)
    return;
  /* Does this replace d->fab.fab$l_fna with a new string in its own space?
     I sure hope so, since p is free'ed before this routine returns. */

  /* have qualified wild name (i.e., disk:[dir.subdir]*.*); null-terminate
   * and set wild-flag */
  d->d_qualwildname[d->nam.nam$b_esl] = '\0';
  d->d_wild = (d->nam.nam$l_fnb & NAM$M_WILDCARD)? 1 : 0;   /* not used... */
#ifdef DEBUG
  fprintf(mesg, "  incoming wildname:  %s\n", p);
  fprintf(mesg, "  qualified wildname:  %s\n", d->d_qualwildname);
#endif /* DEBUG */
}

local zDIR *zopendir(n)
ZCONST char *n;         /* directory to open */
/* Start searching for files in the VMS directory n */
{
  char *c;              /* scans VMS path */
  zDIR *d;              /* malloc'd return value */
  int m;                /* length of name */
  char *p;              /* malloc'd temporary string */

  if ((d = (zDIR *)malloc(sizeof(zDIR))) == NULL ||
      (p = malloc((m = strlen(n)) + 4)) == NULL) {
    if (d != NULL) free((zvoid *)d);
    return NULL;
  }
  /* Directory may be in form "[DIR.SUB1.SUB2]" or "[DIR.SUB1]SUB2.DIR;1".
     If latter, convert to former. */
  if (m > 0  &&  *(c = strcpy(p,n)+m-1) != ']')
  {
    while (--c > p  &&  *c != ';')
      ;
    if (c-p < 5  ||  strncmp(c-4, ".DIR", 4))
    {
      free((zvoid *)d);  free((zvoid *)p);
      return NULL;
    }
    c -= 3;
    *c-- = '\0';        /* terminate at "DIR;#" */
    *c = ']';           /* "." --> "]" */
    while (c > p  &&  *--c != ']')
      ;
    *c = '.';           /* "]" --> "." */
  }
  strcat(p, "*.*");
  strcat(p, wild_version_part);
  vms_wild(p, d);       /* set up wildcard */
  free((zvoid *)p);
  return d;
}

local char *readd(d)
zDIR *d;                /* directory stream to read from */
/* Return a pointer to the next name in the directory stream d, or NULL if
   no more entries or an error occurs. */
{
  int r;                /* return code */

  do {
    d->fab.fab$w_ifi = 0;       /* internal file index:  what does this do? */

    /* get next match to possible wildcard */
    if ((r = sys$search(&d->fab)) == RMS$_NORMAL)
    {
        d->d_name[d->nam.nam$b_rsl] = '\0';   /* null terminate */
        return (char *)d->d_name;   /* OK */
    }
  } while (r == RMS$_PRV);
  return NULL;
}

int wild(p)
char *p;                /* path/pattern to match */
/* Expand the pattern based on the contents of the file system.  Return an
   error code in the ZE_ class. */
{
  zDIR *d;              /* stream for reading directory */
  char *e;              /* name found in directory */
  int f;                /* true if there was a match */

  /* special handling of stdin request */
  if (strcmp(p, "-") == 0)   /* if compressing stdin */
    return newname(p, 0, 0);

  /* Search given pattern for matching names */
  if ((d = (zDIR *)malloc(sizeof(zDIR))) == NULL)
    return ZE_MEM;
  vms_wild(p, d);       /* pattern may be more than just directory name */

  /*
   * Save version specified by user to use in recursive drops into
   * subdirectories.
   */
  strncpy(wild_version_part,d->nam.nam$l_ver,d->nam.nam$b_ver);
  wild_version_part[d->nam.nam$b_ver] = '\0';

  f = 0;
  while ((e = readd(d)) != NULL)        /* "dosmatch" is already built in */
    if (procname(e, 0) == ZE_OK)
      f = 1;
  free(d);

  /* Done */
  return f ? ZE_OK : ZE_MISS;
}

int procname(n, caseflag)
char *n;                /* name to process */
int caseflag;           /* true to force case-sensitive match */
/* Process a name or sh expression to operate on (or exclude).  Return
   an error code in the ZE_ class. */
{
  zDIR *d;              /* directory stream from zopendir() */
  char *e;              /* pointer to name from readd() */
  int m;                /* matched flag */
  char *p;              /* path for recursion */
  struct stat s;        /* result of stat() */
  struct zlist far *z;  /* steps through zfiles list */

  if (strcmp(n, "-") == 0)   /* if compressing stdin */
    return newname(n, 0, caseflag);
  else if (LSSTAT(n, &s)
#if defined(__TURBOC__) || defined(VMS) || defined(__WATCOMC__)
           /* For these 3 compilers, stat() succeeds on wild card names! */
           || isshexp(n)
#endif
          )
  {
    /* Not a file or directory--search for shell expression in zip file */
    if (caseflag) {
      p = malloc(strlen(n) + 1);
      if (p != NULL)
        strcpy(p, n);
    } else
      p = ex2in(n, 0, (int *)NULL);     /* shouldn't affect matching chars */
    m = 1;
    for (z = zfiles; z != NULL; z = z->nxt) {
      if (MATCH(p, z->iname, caseflag))
      {
        z->mark = pcount ? filter(z->zname, caseflag) : 1;
        if (verbose)
            fprintf(mesg, "zip diagnostic: %scluding %s\n",
               z->mark ? "in" : "ex", z->name);
        m = 0;
      }
    }
    free((zvoid *)p);
    return m ? ZE_MISS : ZE_OK;
  }

  /* Live name--use if file, recurse if directory */
  if ((s.st_mode & S_IFDIR) == 0)
  {
    /* add or remove name of file */
    if ((m = newname(n, 0, caseflag)) != ZE_OK)
      return m;
  } else {
    if (dirnames && (m = newname(n, 1, caseflag)) != ZE_OK) {
      return m;
    }
    /* recurse into directory */
    if (recurse && (d = zopendir(n)) != NULL)
    {
      while ((e = readd(d)) != NULL) {
        if ((m = procname(e, caseflag)) != ZE_OK)     /* recurse on name */
        {
          free(d);
          return m;
        }
      }
      free(d);
    }
  } /* (s.st_mode & S_IFDIR) == 0) */
  return ZE_OK;
}

local char *strlower(s)
char *s;                /* string to convert */
/* Convert all uppercase letters to lowercase in string s */
{
  char *p;              /* scans string */

  for (p = s; *p; p++)
    if (*p >= 'A' && *p <= 'Z')
      *p += 'a' - 'A';
  return s;
}

local char *strupper(s)
char *s;                /* string to convert */
/* Convert all lowercase letters to uppercase in string s */
{
  char *p;              /* scans string */

  for (p = s; *p; p++)
    if (*p >= 'a' && *p <= 'z')
      *p -= 'a' - 'A';
  return s;
}

char *ex2in(x, isdir, pdosflag)
char *x;                /* external file name */
int isdir;              /* input: x is a directory */
int *pdosflag;          /* output: force MSDOS file attributes? */
/* Convert the external file name to a zip file name, returning the malloc'ed
   string or NULL if not enough memory. */
{
  char *n;              /* internal file name (malloc'ed) */
  char *t;              /* shortened name */
  int dosflag;

  dosflag = dosify; /* default for non-DOS and non-OS/2 */

  /* Find starting point in name before doing malloc */
  t = x;
  if ((n = strrchr(t, ':')) != NULL)
    t = n + 1;
  if ( (*t == PATH_START && (n = strrchr(t, PATH_END)) != NULL)
      || (*t == PATH_START2 && (n = strrchr(t, PATH_END2)) != NULL) )
    /* external name contains valid VMS path specification */
    if (*(++t) == '.')
      /* path is relative to current directory, skip leading '.' */
      t++;

  if (!pathput)
    t = last(last(t, PATH_END), PATH_END2);

  /* Malloc space for internal name and copy it */
  if ((n = malloc(strlen(t) + 1)) == NULL)
    return NULL;
  strcpy(n, t);

  if (((t = strrchr(n, PATH_END)) != NULL) ||
       (t = strrchr(n, PATH_END2)) != NULL)
  {
    *t = '/';
    while (--t > n)
      if (*t == '.')
        *t = '/';
  }

  /* Fix from Greg Roelofs: */
  /* Get current working directory and strip from n (t now = n) */
  {
    char cwd[256], *p, *q;
    int c;

#if 0 /* fix by Igor */
    if (getcwd(cwd, 256) && ((p = strchr(cwd, '.')) != NULL))
#else
    if (getcwd(cwd, 256) && ((p = strchr(cwd, PATH_START)) != NULL ||
                             (p = strchr(cwd, PATH_START2)) != NULL))
#endif
    {
      if (*(++p) == '.')
        p++;
      if ((q = strrchr(p, PATH_END)) != NULL ||
          (q = strrchr(p, PATH_END2)) != NULL)
      {
        *q = '/';
        while (--q > p)
          if (*q == '.')
            *q = '/';

        /* strip bogus path parts from n */
        if (strncmp(n, p, (c=strlen(p))) == 0)
        {
          q = n + c;
          while (*t++ = *q++)
            ;
        }
      }
    }
  }
  strlower(n);

  if (isdir)
  {
    if (strcmp((t=n+strlen(n)-6), ".dir;1"))
      error("directory not version 1");
    else
      if (pathput)
        strcpy(t, "/");
      else
        *n = '\0';              /* directories are discarded with zip -rj */
  }
  else if (!vmsver)
    if ((t = strrchr(n, ';')) != NULL)
      *t = '\0';

  if ((t = strrchr(n, '.')) != NULL)
  {
    if ( t[1] == '\0')          /* "filename." -> "filename" */
      *t = '\0';
    else if (t[1] == ';')       /* "filename.;vvv" -> "filename;vvv" */
    {
      char *f = t+1;
      while (*t++ = *f++) ;
    }
  }

  if (dosify)
    msname(n);

  /* Returned malloc'ed name */
  if (pdosflag)
    *pdosflag = dosflag;
  return n;
}


char *in2ex(n)
char *n;                /* internal file name */
/* Convert the zip file name to an external file name, returning the malloc'ed
   string or NULL if not enough memory. */
{
  char *x;              /* external file name */
  char *t;              /* scans name */

  if ((t = strrchr(n, '/')) == NULL)
  {
    if ((x = malloc(strlen(n) + 1 + PAD)) == NULL)
      return NULL;
    strcpy(x, n);
  }
  else
  {
    if ((x = malloc(strlen(n) + 3 + PAD)) == NULL)
      return NULL;
    x[0] = PATH_START;
    x[1] = '.';
    strcpy(x + 2, n);
    *(t = x + 2 + (t - n)) = PATH_END;
    while (--t > x)
      if (*t == '/')
        *t = '.';
  }
  strupper(x);

  return x;
}

void stamp(f, d)
char *f;                /* name of file to change */
ulg d;                  /* dos-style time to change it to */
/* Set last updated and accessed time of file f to the DOS time d. */
{
  int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
  char timbuf[24];
  static ZCONST char *month[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                 "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  struct VMStimbuf {
      char *actime;           /* VMS revision date, ASCII format */
      char *modtime;          /* VMS creation date, ASCII format */
  } ascii_times;

  ascii_times.actime = ascii_times.modtime = timbuf;

  /* Convert DOS time to ASCII format for VMSmunch */
  tm_sec = (int)(d << 1) & 0x3e;
  tm_min = (int)(d >> 5) & 0x3f;
  tm_hour = (int)(d >> 11) & 0x1f;
  tm_mday = (int)(d >> 16) & 0x1f;
  tm_mon = ((int)(d >> 21) & 0xf) - 1;
  tm_year = ((int)(d >> 25) & 0x7f) + 1980;
  sprintf(timbuf, "%02d-%3s-%04d %02d:%02d:%02d.00", tm_mday, month[tm_mon],
    tm_year, tm_hour, tm_min, tm_sec);

  /* Set updated and accessed times of f */
  if (VMSmunch(f, SET_TIMES, (char *)&ascii_times) != RMS$_NMF)
    zipwarn("can't set zipfile time: ", f);
}

ulg filetime(f, a, n, t)
char *f;                /* name of file to get info on */
ulg *a;                 /* return value: file attributes */
long *n;                /* return value: file size */
iztimes *t;             /* return value: access, modific. and creation times */
/* If file *f does not exist, return 0.  Else, return the file's last
   modified date and time as an MSDOS date and time.  The date and
   time is returned in a long with the date most significant to allow
   unsigned integer comparison of absolute times.  Also, if a is not
   a NULL pointer, store the file attributes there, with the high two
   bytes being the Unix attributes, and the low byte being a mapping
   of that to DOS attributes.  If n is not NULL, store the file size
   there.  If t is not NULL, the file's access, modification and creation
   times are stored there as UNIX time_t values.
   If f is "-", use standard input as the file. If f is a device, return
   a file size of -1 */
{
  struct stat s;        /* results of stat() */
  char name[FNMAX];
  int len = strlen(f);

  if (f == label) {
    if (a != NULL)
      *a = label_mode;
    if (n != NULL)
      *n = -2L; /* convention for a label name */
    if (t != NULL)
      t->atime = t->mtime = t->ctime = label_utim;
    return label_time;
  }
  strcpy(name, f);
  if (name[len - 1] == '/')
    name[len - 1] = '\0';
  /* not all systems allow stat'ing a file with / appended */

  if (strcmp(f, "-") == 0) {
    if (fstat(fileno(stdin), &s) != 0)
      error("fstat(stdin)");
  } else if (LSSTAT(name, &s) != 0)
             /* Accept about any file kind including directories
              * (stored with trailing / with -r option)
              */
    return 0;

  if (a != NULL) {
    *a = ((ulg)s.st_mode << 16) | !(s.st_mode & S_IWRITE);
    if ((s.st_mode & S_IFDIR) != 0) {
      *a |= MSDOS_DIR_ATTR;
    }
  }
  if (n != NULL)
    *n = (s.st_mode & S_IFMT) == S_IFREG ? s.st_size : -1L;
  if (t != NULL) {
    t->atime = s.st_mtime;
#ifdef USE_MTIME
    t->mtime = s.st_mtime;            /* Use modification time in VMS */
#else
    t->mtime = s.st_ctime;            /* Use creation time in VMS */
#endif
    t->ctime = s.st_ctime;
  }

#ifdef USE_MTIME
  return unix2dostime((time_t *)&s.st_mtime); /* Use modification time in VMS */
#else
  return unix2dostime((time_t *)&s.st_ctime); /* Use creation time in VMS */
#endif
}

int deletedir(d)
char *d;                /* directory to delete */
/* Delete the directory *d if it is empty, do nothing otherwise.
   Return the result of rmdir(), delete(), or system().
   For VMS, d must be in format [x.y]z.dir;1  (not [x.y.z]).
 */
{
    /* code from Greg Roelofs, who horked it from Mark Edwards (unzip) */
    int r, len;
    char *s;              /* malloc'd string for system command */

    len = strlen(d);
    if ((s = malloc(len + 34)) == NULL)
      return 127;

    system(strcat(strcpy(s, "set prot=(o:rwed) "), d));
    r = delete(d);
    free(s);
    return r;
}

#endif /* !UTIL */
