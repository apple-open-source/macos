/*
 * glob.c - filename generation
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1996 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

#include "zsh.h"

static int
exists(char *s)
{
    char *us = unmeta(s);

    return access(us,0) == 0 || readlink(us,NULL,0) == 0;
}

static int mode;		/* != 0 if we are parsing glob patterns */
static int pathpos;		/* position in pathbuf                  */
static int matchsz;		/* size of matchbuf                     */
static int matchct;		/* number of matches found              */
static char pathbuf[PATH_MAX];	/* pathname buffer                      */
static char **matchbuf;		/* array of matches                     */
static char **matchptr;		/* &matchbuf[matchct]                   */
static char *colonmod;		/* colon modifiers in qualifier list    */
static ino_t old_ino;		/* ) remember old file and              */
static dev_t old_dev;		/* ) position in path in case           */
static int old_pos;		/* ) matching multiple directories      */

typedef struct stat *Statptr;	 /* This makes the Ultrix compiler happy.  Go figure. */

/* modifier for unit conversions */

#define TT_DAYS 0
#define TT_HOURS 1
#define TT_MINS 2
#define TT_WEEKS 3
#define TT_MONTHS 4
#define TT_SECONDS 5

#define TT_BYTES 0
#define TT_POSIX_BLOCKS 1
#define TT_KILOBYTES 2
#define TT_MEGABYTES 3

/* max # of qualifiers */

typedef int (*TestMatchFunc) _((struct stat *, off_t));

struct qual {
    struct qual *next;		/* Next qualifier, must match                */
    struct qual *or;		/* Alternative set of qualifiers to match    */
    TestMatchFunc func;		/* Function to call to test match            */
    off_t data;			/* Argument passed to function               */
    int sense;			/* Whether asserting or negating             */
    int amc;			/* Flag for which time to test (a, m, c)     */
    int range;			/* Whether to test <, > or = (as per signum) */
    int units;			/* Multiplier for time or size, respectively */
};

/* Qualifiers pertaining to current pattern */
static struct qual *quals;

/* Other state values for current pattern */
static int qualct, qualorct;
static int range, amc, units;
static int gf_nullglob, gf_markdirs, gf_noglobdots, gf_listtypes, gf_follow;

/* Prefix, suffix for doing zle trickery */
char *glob_pre, *glob_suf;

/* pathname component in filename patterns */

struct complist {
    Complist next;
    Comp comp;
    int closure;		/* 1 if this is a (foo/)# */
    int follow; 		/* 1 to go thru symlinks */
};
struct comp {
    Comp left, right, next, exclude;
    char *str;
    int stat;
};

/* Type of Comp:  a closure with one or two #'s, the end of a *
 * pattern or path component, a piece of path to be added.    */
#define C_ONEHASH	1
#define C_TWOHASH	2
#define C_CLOSURE	(C_ONEHASH|C_TWOHASH)
#define C_LAST		4
#define C_PATHADD	8

/* Test macros for the above */
#define CLOSUREP(c)	(c->stat & C_CLOSURE)
#define ONEHASHP(c)	(c->stat & C_ONEHASH)
#define TWOHASHP(c)	(c->stat & C_TWOHASH)
#define LASTP(c)	(c->stat & C_LAST)
#define PATHADDP(c)	(c->stat & C_PATHADD)

/* Main entry point to the globbing code for filename globbing. *
 * np points to a node in the list list which will be expanded  *
 * into a series of nodes.                                      */

/**/
void
glob(LinkList list, LinkNode np)
{
    struct qual *qo, *qn, *ql;
    LinkNode node = prevnode(np);
    char *str;				/* the pattern                   */
    int sl;				/* length of the pattern         */
    Complist q;				/* pattern after parsing         */
    char *ostr = (char *)getdata(np);	/* the pattern before the parser */
					/* chops it up                   */

    MUSTUSEHEAP("glob");
    if (unset(GLOBOPT) || !(str = haswilds(ostr))) {
	untokenize(ostr);
	return;
    }
    if (*str == ':' && str[-1] == Inpar) {
	str[-1] = '\0';
	str[strlen(str)-1] = '\0';
	untokenize(str);
	modify((char **) &getdata(np), &str);
	return;
    }
    str = dupstring(ostr);
    sl = strlen(str);
    uremnode(list, np);

    /* Initialise state variables for current file pattern */
    qo = qn = quals = ql = NULL;
    qualct = qualorct = 0;
    colonmod = NULL;
    gf_nullglob = isset(NULLGLOB);
    gf_markdirs = isset(MARKDIRS);
    gf_listtypes = gf_follow = 0;
    gf_noglobdots = unset(GLOBDOTS);
    if (str[sl - 1] == Outpar) {	/* check for qualifiers */
	char *s;
	int sense = 0;			/* bit 0 for match (0)/don't match (1)   */
					/* bit 1 for follow links (2), don't (0) */
	off_t data = 0;			/* Any numerical argument required       */

	int (*func) _((Statptr, off_t));

	/* Check these are really qualifiers, not a set of *
	 * alternatives or exclusions                      */
	for (s = str + sl - 2; s != str; s--)
	    if (*s == Bar || *s == Outpar || *s == Inpar
		|| (isset(EXTENDEDGLOB) && *s == Tilde))
		break;
	if (*s == Inpar) {
	    /* Real qualifiers found. */
	    str[sl-1] = '\0';
	    *s++ = '\0';
	    while (*s && !colonmod) {
		func = (int (*) _((Statptr, off_t)))0;
		if (idigit(*s)) {
		    /* Store numeric argument for qualifier */
		    func = qualflags;
		    data = 0;
		    while (idigit(*s))
			data = data * 010 + (*s++ - '0');
		} else if (*s == ',') {
		    /* A comma separates alternative sets of qualifiers */
		    s++;
		    sense = 0;
		    if (qualct) {
			qn = (struct qual *)hcalloc(sizeof *qn);
			qo->or = qn;
			qo = qn;
			qualorct++;
			qualct = 0;
			ql = NULL;
		    }
		} else
		    switch (*s++) {
		    case ':':
			/* Remaining arguments are history-type     *
			 * colon substitutions, handled separately. */
			colonmod = s - 1;
			untokenize(colonmod);
			break;
		    case Hat:
		    case '^':
			/* Toggle sense:  go from positive to *
			 * negative match and vice versa.     */
			sense ^= 1;
			break;
		    case '-':
			/* Toggle matching of symbolic links */
			sense ^= 2;
			break;
#ifdef S_IFLNK
		    case '@':
			/* Match symbolic links */
			func = qualmode;
			data = S_IFLNK;
			break;
#endif
#ifdef S_IFSOCK
		    case Equals:
		    case '=':
			/* Match sockets */
			func = qualmode;
			data = S_IFSOCK;
			break;
#endif
#ifdef S_IFIFO
		    case 'p':
			/* Match named pipes */
			func = qualmode;
			data = S_IFIFO;
			break;
#endif
		    case '/':
			/* Match directories */
			func = qualmode;
			data = S_IFDIR;
			break;
		    case '.':
			/* Match regular files */
			func = qualmode;
			data = S_IFREG;
			break;
		    case '%':
			/* Match special files: block, *
			 * character or any device     */
			if (*s == 'b')
			    s++, func = qualisblk;
			else if (*s == 'c')
			    s++, func = qualischar;
			else
			    func = qualisdev;
			break;
		    case Star:
			/* Match executable plain files */
			func = qualiscom;
			break;
		    case 'R':
			/* Match world-readable files */
			func = qualflags;
			data = 0004;
			break;
		    case 'W':
			/* Match world-writeable files */
			func = qualflags;
			data = 0002;
			break;
		    case 'X':
			/* Match world-executable files */
			func = qualflags;
			data = 0001;
			break;
		    case 'A':
			func = qualflags;
			data = 0040;
			break;
		    case 'I':
			func = qualflags;
			data = 0020;
			break;
		    case 'E':
			func = qualflags;
			data = 0010;
			break;
		    case 'r':
			/* Match files readable by current process */
			func = qualflags;
			data = 0400;
			break;
		    case 'w':
			/* Match files writeable by current process */
			func = qualflags;
			data = 0200;
			break;
		    case 'x':
			/* Match files executable by current process */
			func = qualflags;
			data = 0100;
			break;
		    case 's':
			/* Match setuid files */
			func = qualflags;
			data = 04000;
			break;
		    case 'S':
			/* Match setgid files */
			func = qualflags;
			data = 02000;
			break;
		    case 't':
			func = qualflags;
			data = 01000;
			break;
		    case 'd':
			/* Match device files by device number  *
			 * (as given by stat's st_dev element). */
			func = qualdev;
			data = qgetnum(&s);
			break;
		    case 'l':
			/* Match files with the given no. of hard links */
			func = qualnlink;
			amc = -1;
			goto getrange;
		    case 'U':
			/* Match files owned by effective user ID */
			func = qualuid;
			data = geteuid();
			break;
		    case 'G':
			/* Match files owned by effective group ID */
			func = qualgid;
			data = getegid();
			break;
		    case 'u':
			/* Match files owned by given user id */
			func = qualuid;
			/* either the actual uid... */
			if (idigit(*s))
			    data = qgetnum(&s);
			else {
			    /* ... or a user name */
			    struct passwd *pw;
			    char sav, *tt;

			    /* Find matching delimiters */
			    tt = get_strarg(s);
			    if (!*tt) {
				zerr("missing end of name",
				     NULL, 0);
				data = 0;
			    } else {
				sav = *tt;
				*tt = '\0';

				if ((pw = getpwnam(s + 1)))
				    data = pw->pw_uid;
				else {
				    zerr("unknown user", NULL, 0);
				    data = 0;
				}
				if ((*tt = sav))
				    s = tt + 1;
				else
				    s = tt;
			    }
			}
			break;
		    case 'g':
			/* Given gid or group id... works like `u' */
			func = qualgid;
			/* either the actual gid... */
			if (idigit(*s))
			    data = qgetnum(&s);
			else {
			    /* ...or a delimited group name. */
			    struct group *gr;
			    char sav, *tt;

			    tt = get_strarg(s);
			    if (!*tt) {
				zerr("missing end of name",
				     NULL, 0);
				data = 0;
			    } else {
				sav = *tt;
				*tt = '\0';

				if ((gr = getgrnam(s + 1)))
				    data = gr->gr_gid;
				else {
				    zerr("unknown group", NULL, 0);
				    data = 0;
				}
				if ((*tt = sav))
				    s = tt + 1;
				else
				    s = tt;
			    }
			}
			break;
		    case 'o':
			/* Match octal mode of file exactly. *
			 * Currently undocumented.           */
			func = qualeqflags;
			data = qgetoctnum(&s);
			break;
		    case 'M':
			/* Mark directories with a / */
			if ((gf_markdirs = !(sense & 1)))
			    gf_follow = sense & 2;
			break;
		    case 'T':
			/* Mark types in a `ls -F' type fashion */
			if ((gf_listtypes = !(sense & 1)))
			    gf_follow = sense & 2;
			break;
		    case 'N':
			/* Nullglob:  remove unmatched patterns. */
			gf_nullglob = !(sense & 1);
			break;
		    case 'D':
			/* Glob dots: match leading dots implicitly */
			gf_noglobdots = sense & 1;
			break;
		    case 'a':
			/* Access time in given range */
			amc = 0;
			func = qualtime;
			goto getrange;
		    case 'm':
			/* Modification time in given range */
			amc = 1;
			func = qualtime;
			goto getrange;
		    case 'c':
			/* Inode creation time in given range */
			amc = 2;
			func = qualtime;
			goto getrange;
		    case 'L':
			/* File size (Length) in given range */
			func = qualsize;
			amc = -1;
			/* Get size multiplier */
			units = TT_BYTES;
			if (*s == 'p' || *s == 'P')
			    units = TT_POSIX_BLOCKS, ++s;
			else if (*s == 'k' || *s == 'K')
			    units = TT_KILOBYTES, ++s;
			else if (*s == 'm' || *s == 'M')
			    units = TT_MEGABYTES, ++s;
		      getrange:
			/* Get time multiplier */
			if (amc >= 0) {
			    units = TT_DAYS;
			    if (*s == 'h')
				units = TT_HOURS, ++s;
			    else if (*s == 'm')
				units = TT_MINS, ++s;
			    else if (*s == 'w')
				units = TT_WEEKS, ++s;
			    else if (*s == 'M')
				units = TT_MONTHS, ++s;
			    else if (*s == 's')
				units = TT_SECONDS, ++s;
			}
			/* See if it's greater than, equal to, or less than */
			if ((range = *s == '+' ? 1 : *s == '-' ? -1 : 0))
			    ++s;
			data = qgetnum(&s);
			break;

		    default:
			zerr("unknown file attribute", NULL, 0);
			return;
		    }
		if (func) {
		    /* Requested test is performed by function func */
		    if (!qn)
			qn = (struct qual *)hcalloc(sizeof *qn);
		    if (ql)
			ql->next = qn;
		    ql = qn;
		    if (!quals)
			quals = qo = qn;
		    qn->func = func;
		    qn->sense = sense;
		    qn->data = data;
		    qn->range = range;
		    qn->units = units;
		    qn->amc = amc;
		    qn = NULL;
		    qualct++;
		}
		if (errflag)
		    return;
	    }
	}
    }
    if (*str == '/') {		/* pattern has absolute path */
	str++;
	pathbuf[0] = '/';
	pathbuf[pathpos = 1] = '\0';
    } else			/* pattern is relative to pwd */
	pathbuf[pathpos = 0] = '\0';
    q = parsepat(str);
    if (!q || errflag) {	/* if parsing failed */
	if (unset(BADPATTERN)) {
	    untokenize(ostr);
	    insertlinknode(list, node, ostr);
	    return;
	}
	errflag = 0;
	zerr("bad pattern: %s", ostr, 0);
	return;
    }

    /* Initialise receptacle for matched files, *
     * expanded by insert() where necessary.    */
    matchptr = matchbuf = (char **)zalloc((matchsz = 16) * sizeof(char *));
    matchct = 0;

    /* Initialise memory of last file matched */
    old_ino = (ino_t) 0;
    old_dev = (dev_t) 0;
    old_pos = -1;

    /* The actual processing takes place here: matches go into  *
     * matchbuf.  This is the only top-level call to scanner(). */
    scanner(q);

    /* Deal with failures to match depending on options */
    if (matchct)
	badcshglob |= 2;	/* at least one cmd. line expansion O.K. */
    else if (!gf_nullglob) {
	if (isset(CSHNULLGLOB)) {
	    badcshglob |= 1;	/* at least one cmd. line expansion failed */
	} else if (isset(NOMATCH)) {
	    zerr("no matches found: %s", ostr, 0);
	    free(matchbuf);
	    return;
	} else {
	    /* treat as an ordinary string */
	    untokenize(*matchptr++ = dupstring(ostr));
	    matchct = 1;
	}
    }
    /* Sort arguments in to lexical (and possibly numeric) order. *
     * This is reversed to facilitate insertion into the list.    */
    qsort((void *) & matchbuf[0], matchct, sizeof(char *),
	       (int (*) _((const void *, const void *)))notstrcmp);

    matchptr = matchbuf;
    while (matchct--)		/* insert matches in the arg list */
	insertlinknode(list, node, *matchptr++);
    free(matchbuf);
}

/* get number after qualifier */

/**/
off_t
qgetnum(char **s)
{
    off_t v = 0;

    if (!idigit(**s)) {
	zerr("number expected", NULL, 0);
	return 0;
    }
    while (idigit(**s))
	v = v * 10 + *(*s)++ - '0';
    return v;
}

/* get octal number after qualifier */

/**/
off_t
qgetoctnum(char **s)
{
    off_t v = 0;

    if (!idigit(**s)) {
	zerr("octal number expected", NULL, 0);
	return 0;
    }
    while (**s >= '0' && **s <= '7')
	v = v * 010 + *(*s)++ - '0';
    return v;
}

/* Return the order of two strings, taking into account *
 * possible numeric order if NUMERICGLOBSORT is set.    *
 * The comparison here is reversed.                     */

/**/
int
notstrcmp(char **a, char **b)
{
    char *c = *b, *d = *a;
    int cmp;

#ifdef HAVE_STRCOLL
    cmp = strcoll(c, d);
#endif
    for (; *c == *d && *c; c++, d++);
#ifndef HAVE_STRCOLL
    cmp = (int)STOUC(*c) - (int)STOUC(*d);
#endif
    if (isset(NUMERICGLOBSORT) && (idigit(*c) || idigit(*d))) {
	for (; c > *b && idigit(c[-1]); c--, d--);
	if (idigit(*c) && idigit(*d)) {
	    while (*c == '0')
		c++;
	    while (*d == '0')
		d++;
	    for (; idigit(*c) && *c == *d; c++, d++);
	    if (idigit(*c) || idigit(*d)) {
		cmp = (int)STOUC(*c) - (int)STOUC(*d);
		while (idigit(*c) && idigit(*d))
		    c++, d++;
		if (idigit(*c) && !idigit(*d))
		    return 1;
		if (idigit(*d) && !idigit(*c))
		    return -1;
	    }
	}
    }
    return cmp;
}

/* add a match to the list */

/**/
void
insert(char *s)
{
    struct stat buf, buf2, *bp;
    char *news = s;
    int statted = 0;

    if (gf_listtypes || gf_markdirs) {
	/* Add the type marker to the end of the filename */
	statted = -1;
	if (!lstat(unmeta(s), &buf)) {
	    mode_t mode = buf.st_mode;
	    statted = 1;
	    if (gf_follow) {
		if (!S_ISLNK(mode) || stat(unmeta(s), &buf2))
		    memcpy(&buf2, &buf, sizeof(buf));
		statted = 2;
		mode = buf2.st_mode;
	    }
	    if (gf_listtypes || S_ISDIR(mode)) {
		int ll = strlen(s);

		news = (char *)ncalloc(ll + 2);
		strcpy(news, s);
		news[ll] = file_type(buf.st_mode);
		news[ll + 1] = '\0';
	    }
	}
    }
    if (qualct || qualorct) {
	/* Go through the qualifiers, rejecting the file if appropriate */
	struct qual *qo, *qn;
	int t = 0;		/* reject file unless t is set */

	if (statted >= 0 && (statted || !lstat(unmeta(s), &buf))) {
	    for (qo = quals; qo && !t; qo = qo->or) {

		t = 1;
		for (qn = qo; t && qn && qn->func; qn = qn->next) {
		    range = qn->range;
		    amc = qn->amc;
		    units = qn->units;
		    if ((qn->sense & 2) && statted != 2) {
			/* If (sense & 2), we're following links */
			if (!S_ISLNK(buf.st_mode) || stat(unmeta(s), &buf2))
			    memcpy(&buf2, &buf, sizeof(buf));
			statted = 2;
		    }
		    bp = (qn->sense & 2) ? &buf2 : &buf;
		    /* Reject the file if the function returned zero *
		     * and the sense was positive (sense&1 == 0), or *
		     * vice versa.                                   */
		    if ((!((qn->func) (bp, qn->data)) ^ qn->sense) & 1) {
			t = 0;
			break;
		    }
		}
	    }
	}
	if (!t)
	    return;
    }
    if (colonmod) {
	/* Handle the remainder of the qualifer:  e.g. (:r:s/foo/bar/). */
	s = colonmod;
	modify(&news, &s);
    }
    *matchptr++ = news;
    if (++matchct == matchsz) {
	matchbuf = (char **)realloc((char *)matchbuf,
				    sizeof(char **) * (matchsz *= 2));

	matchptr = matchbuf + matchct;
    }
}

/* Return the trailing character for marking file types */

/**/
char
file_type(mode_t filemode)
{
    switch (filemode & S_IFMT) {/* screw POSIX */
    case S_IFDIR:
	return '/';
#ifdef S_IFIFO
    case S_IFIFO:
	return '|';
#endif
    case S_IFCHR:
	return '%';
    case S_IFBLK:
	return '#';
#ifdef S_IFLNK
    case S_IFLNK:
	return /* (access(pbuf, F_OK) == -1) ? '&' :*/ '@';
#endif
#ifdef S_IFSOCK
    case S_IFSOCK:
	return '=';
#endif
    default:
	if (filemode & 0111)
	    return '*';
	else
	    return ' ';
    }
}

/* check to see if str is eligible for filename generation
 * It returns NULL if no wilds or modifiers found.
 * If a leading % is immediately followed by a ?, that single
 * ? is not treated as a wildcard.
 * If str has wilds it returns a pointer to the first wildcard.
 * If str has no wilds but ends in a (:...) type modifier it returns
 * a pointer to the colon.
 * If str has no wilds but ends in (...:...) it returns a pointer
 * to the terminating null character of str.
 */

/**/
char *
haswilds(char *str)
{
    char *mod = NULL;
    int parlev = 0;

    if ((*str == Inbrack || *str == Outbrack) && !str[1])
	return NULL;

    /* If % is immediately followed by ?, then that ? is     *
     * not treated as a wildcard.  This is so you don't have *
     * to escape job references such as %?foo.               */
    if (str[0] == '%' && str[1] == Quest)
	str[1] = '?';
    for (; *str; str++)
	switch (*str) {
	case Inpar:
	    parlev++;
	    break;
	case Outpar:
	    if (! --parlev && str[1])
		mod = NULL;
	    break;
	case ':':
	    if (parlev == 1 && !mod)
		mod = str;
	    break;
	case Bar:
	    if (!parlev) {
		*str = '|';
		break;
	    } /* else fall through */
	case Star:
	case Inbrack:
	case Inang:
	case Quest:
	    return str;
	case Pound:
	case Hat:
	    if (isset(EXTENDEDGLOB))
		return str;
	}
    if (!mod || parlev)
	return NULL;
    if (mod[-1] == Inpar)
	return mod;
    return str;
}

/* check to see if str is eligible for brace expansion */

/**/
int
hasbraces(char *str)
{
    char *lbr, *mbr, *comma;

    if (isset(BRACECCL)) {
	/* In this case, any properly formed brace expression  *
	 * will match and expand to the characters in between. */
	int bc;

	for (bc = 0; *str; ++str) {
	    if (*str == Inbrace) {
		if (!bc && str[1] == Outbrace)
		    *str++ = '{', *str = '}';
		else
		    bc++;
	    } else if (*str == Outbrace) {
		if (!bc)
		    *str = '}';
		else if (!--bc)
		    return 1;
	    }
	}
	return 0;
    }
    /* Otherwise we need to look for... */
    lbr = mbr = comma = NULL;
    for (;;) {
	switch (*str++) {
	case Inbrace:
	    if (!lbr) {
		lbr = str - 1;
		while (idigit(*str))
		    str++;
		if (*str == '.' && str[1] == '.') {
		    str++;
		    while (idigit(*++str));
		    if (*str == Outbrace &&
			(idigit(lbr[1]) || idigit(str[-1])))
			return 1;
		}
	    } else {
		char *s = --str;

		if (skipparens(Inbrace, Outbrace, &str)) {
		    *lbr = *s = '{';
		    if (comma)
			str = comma;
		    if (mbr && mbr < str)
			str = mbr;
		    lbr = mbr = comma = NULL;
		} else if (!mbr)
		    mbr = s;
	    }
	    break;
	case Outbrace:
	    if (!lbr)
		str[-1] = '}';
	    else if (comma)
		return 1;
	    else {
		*lbr = '{';
		str[-1] = '}';
		if (mbr)
		    str = mbr;
		mbr = lbr = NULL;
	    }
	    break;
	case Comma:
	    if (!lbr)
		str[-1] = ',';
	    else if (!comma)
		comma = str - 1;
	    break;
	case '\0':
	    if (lbr)
		*lbr = '{';
	    if (!mbr && !comma)
		return 0;
	    if (comma)
		str = comma;
	    if (mbr && mbr < str)
		str = mbr;
	    lbr = mbr = comma = NULL;
	    break;
	}
    }
}

/* expand stuff like >>*.c */

/**/
int
xpandredir(struct redir *fn, LinkList tab)
{
    LinkList fake;
    char *nam;
    struct redir *ff;
    int ret = 0;

    /* Stick the name in a list... */
    fake = newlinklist();
    addlinknode(fake, fn->name);
    /* ...which undergoes all the usual shell expansions */
    prefork(fake, isset(MULTIOS) ? 0 : 4);
    /* Globbing is only done for multios. */
    if (!errflag && isset(MULTIOS))
	globlist(fake);
    if (errflag)
	return 0;
    if (nonempty(fake) && !nextnode(firstnode(fake))) {
	/* Just one match, the usual case. */
	char *s = peekfirst(fake);
	fn->name = s;
	untokenize(s);
	if (fn->type == MERGEIN || fn->type == MERGEOUT) {
	    if (s[0] == '-' && !s[1])
		fn->type = CLOSE;
	    else if (s[0] == 'p' && !s[1]) 
		fn->fd2 = -2;
	    else {
		while (idigit(*s))
		    s++;
		if (!*s && s > fn->name)
		    fn->fd2 = zstrtol(fn->name, NULL, 10);
		else if (fn->type == MERGEIN)
		    zerr("file number expected", NULL, 0);
		else
		    fn->type = ERRWRITE;
	    }
	}
    } else if (fn->type == MERGEIN)
	zerr("file number expected", NULL, 0);
    else {
	if (fn->type == MERGEOUT)
	    fn->type = ERRWRITE;
	while ((nam = (char *)ugetnode(fake))) {
	    /* Loop over matches, duplicating the *
	     * redirection for each file found.   */
	    ff = (struct redir *)alloc(sizeof *ff);
	    *ff = *fn;
	    ff->name = nam;
	    addlinknode(tab, ff);
	    ret = 1;
	}
    }
    return ret;
}

/* concatenate s1 and s2 in dynamically allocated buffer */

/**/
char *
dyncat(char *s1, char *s2)
{
    /* This version always uses space from the current heap. */
    char *ptr;
    int l1 = strlen(s1);

    ptr = (char *)ncalloc(l1 + strlen(s2) + 1);
    strcpy(ptr, s1);
    strcpy(ptr + l1, s2);
    return ptr;
}

/* concatenate s1, s2, and s3 in dynamically allocated buffer */

/**/
char *
tricat(char *s1, char *s2, char *s3)
{
    /* This version always uses permanently-allocated space. */
    char *ptr;

    ptr = (char *)zalloc(strlen(s1) + strlen(s2) + strlen(s3) + 1);
    strcpy(ptr, s1);
    strcat(ptr, s2);
    strcat(ptr, s3);
    return ptr;
}

/* brace expansion */

/**/
void
xpandbraces(LinkList list, LinkNode *np)
{
    LinkNode node = (*np), last = prevnode(node);
    char *str = (char *)getdata(node), *str3 = str, *str2;
    int prev, bc, comma, dotdot;

    for (; *str != Inbrace; str++);
    /* First, match up braces and see what we have. */
    for (str2 = str, bc = comma = dotdot = 0; *str2; ++str2)
	if (*str2 == Inbrace)
	    ++bc;
	else if (*str2 == Outbrace) {
	    if (--bc == 0)
		break;
	} else if (bc == 1) {
	    if (*str2 == Comma)
		++comma;	/* we have {foo,bar} */
	    else if (*str2 == '.' && str2[1] == '.')
		dotdot++;	/* we have {num1..num2} */
	}
    DPUTS(bc, "BUG: unmatched brace in xpandbraces()");
    if (!comma && dotdot) {
	/* Expand range like 0..10 numerically: comma or recursive
	   brace expansion take precedence. */
	char *dots, *p;
	LinkNode olast = last;
	/* Get the first number of the range */
	int rstart = zstrtol(str+1,&dots,10), rend = 0, err = 0, rev = 0;
	int wid1 = (dots - str) - 1, wid2 = (str2 - dots) - 2;
	int strp = str - str3;
      
	if (dots == str + 1 || *dots != '.' || dots[1] != '.')
	    err++;
	else {
	    /* Get the last number of the range */
	    rend = zstrtol(dots+2,&p,10);
	    if (p == dots+2 || p != str2)
		err++;
	}
	if (!err) {
	    /* If either no. begins with a zero, pad the output with   *
	     * zeroes. Otherwise, choose a min width to suppress them. */
	    int minw = (str[1] == '0') ? wid1 : (dots[2] == '0' ) ? wid2 :
		(wid2 > wid1) ? wid1 : wid2;
	    if (rstart > rend) {
		/* Handle decreasing ranges correctly. */
		int rt = rend;
		rend = rstart;
		rstart = rt;
		rev = 1;
	    }
	    uremnode(list, node);
	    for (; rend >= rstart; rend--) {
		/* Node added in at end, so do highest first */
		p = dupstring(str3);
		sprintf(p + strp, "%0*d", minw, rend);
		strcat(p + strp, str2 + 1);
		insertlinknode(list, last, p);
		if (rev)	/* decreasing:  add in reverse order. */
		    last = nextnode(last);
	    }
	    *np = nextnode(olast);
	    return;
	}
    }
    if (!comma && isset(BRACECCL)) {	/* {a-mnop} */
	/* Here we expand each character to a separate node,      *
	 * but also ranges of characters like a-m.  ccl is a      *
	 * set of flags saying whether each character is present; *
	 * the final list is in lexical order.                    */
	char ccl[256], *p;
	unsigned char c1, c2, lastch;
	unsigned int len, pl;

	uremnode(list, node);
	memset(ccl, 0, sizeof(ccl) / sizeof(ccl[0]));
	for (p = str + 1, lastch = 0; p < str2;) {
	    if (itok(c1 = *p++))
		c1 = ztokens[c1 - STOUC(Pound)];
	    if ((char) c1 == Meta)
		c1 = 32 ^ *p++;
	    if (itok(c2 = *p))
		c2 = ztokens[c2 - STOUC(Pound)];
	    if ((char) c2 == Meta)
		c2 = 32 ^ p[1];
	    if (c1 == '-' && lastch && p < str2 && (int)lastch <= (int)c2) {
		while ((int)lastch < (int)c2)
		    ccl[lastch++] = 1;
		lastch = 0;
	    } else
		ccl[lastch = c1] = 1;
	}
	pl = str - str3;
	len = pl + strlen(++str2) + 2;
	for (p = ccl + 255; p-- > ccl;)
	    if (*p) {
		c1 = p - ccl;
		if (imeta(c1)) {
		    str = ncalloc(len + 1);
		    str[pl] = Meta;
		    str[pl+1] = c1 ^ 32;
		    strcpy(str + pl + 2, str2);
		} else {
		    str = ncalloc(len);
		    str[pl] = c1;
		    strcpy(str + pl + 1, str2);
		}
		memcpy(str, str3, pl);
		insertlinknode(list, last, str);
	    }
	*np = nextnode(last);
	return;
    }
    prev = str++ - str3;
    str2++;
    uremnode(list, node);
    node = last;
    /* Finally, normal comma expansion               *
     * str1{foo,bar}str2 -> str1foostr2 str1barstr2. *
     * Any number of intervening commas is allowed.  */
    for (;;) {
	char *zz, *str4;
	int cnt;

	for (str4 = str, cnt = 0; cnt || (*str != Comma && *str !=
					  Outbrace); str++) {
	    if (*str == Inbrace)
		cnt++;
	    else if (*str == Outbrace)
		cnt--;
	    DPUTS(!*str, "BUG: illegal brace expansion");
	}
	/* Concatenate the string before the braces (str3), the section *
	 * just found (str4) and the text after the braces (str2)       */
	zz = (char *)ncalloc(prev + (str - str4) + strlen(str2) + 1);
	ztrncpy(zz, str3, prev);
	strncat(zz, str4, str - str4);
	strcat(zz, str2);
	/* and add this text to the argument list. */
	insertlinknode(list, node, zz);
	incnode(node);
	if (*str != Outbrace)
	    str++;
	else
	    break;
    }
    *np = nextnode(last);
}

/* check to see if a matches b (b is not a filename pattern) */

/**/
int
matchpat(char *a, char *b)
{
    Comp c;
    int val, len;
    char *b2;

    len = strlen(b);
    b2 = (char *)alloc(len + 3);
    strcpy(b2 + 1, b);
    b2[0] = Inpar;
    b2[len + 1] = Outpar;
    b2[len + 2] = '\0';
    c = parsereg(b2);
    if (!c) {
	zerr("bad pattern: %s", b, 0);
	return 0;
    }
    val = domatch(a, c, 0);
    return val;
}

/* do the ${foo%%bar}, ${foo#bar} stuff */
/* please do not laugh at this code. */

/* Having found a match in getmatch, decide what part of string
 * to return.  The matched part starts b characters into string s
 * and finishes e characters in: 0 <= b <= e <= strlen(s)
 * (yes, empty matches should work).
 * Bits 3 and higher in fl are used: the flags are
 *   8:		Result is matched portion.
 *  16:		Result is unmatched portion.
 *		(N.B. this should be set for standard ${foo#bar} etc. matches.)
 *  32:		Result is numeric position of start of matched portion.
 *  64:		Result is numeric position of end of matched portion.
 * 128:		Result is length of matched portion.
 */

/**/
char *
get_match_ret(char *s, int b, int e, int fl)
{
    char buf[80], *r, *p, *rr;
    int ll = 0, l = strlen(s), bl = 0, t = 0, i;

    if (fl & 8)			/* matched portion */
	ll += 1 + (e - b);
    if (fl & 16)		/* unmatched portion */
	ll += 1 + (l - (e - b));
    if (fl & 32) {
	/* position of start of matched portion */
	sprintf(buf, "%d ", b + 1);
	ll += (bl = strlen(buf));
    }
    if (fl & 64) {
	/* position of end of matched portion */
	sprintf(buf + bl, "%d ", e + 1);
	ll += (bl = strlen(buf));
    }
    if (fl & 128) {
	/* length of matched portion */
	sprintf(buf + bl, "%d ", e - b);
	ll += (bl = strlen(buf));
    }
    if (bl)
	buf[bl - 1] = '\0';

    rr = r = (char *)ncalloc(ll);

    if (fl & 8) {
	/* copy matched portion to new buffer */
	for (i = b, p = s + b; i < e; i++)
	    *rr++ = *p++;
	t = 1;
    }
    if (fl & 16) {
	/* Copy unmatched portion to buffer.  If both portions *
	 * requested, put a space in between (why?)            */
	if (t)
	    *rr++ = ' ';
	/* there may be unmatched bits at both beginning and end of string */
	for (i = 0, p = s; i < b; i++)
	    *rr++ = *p++;
	for (i = e, p = s + e; i < l; i++)
	    *rr++ = *p++;
	t = 1;
    }
    *rr = '\0';
    if (bl) {
	/* if there was a buffer (with a numeric result), add it; *
	 * if there was other stuff too, stick in a space first.  */
	if (t)
	    *rr++ = ' ';
	strcpy(rr, buf);
    }
    return r;
}

/* It is called from paramsubst to get the match for ${foo#bar} etc.
 * Bits of fl determines the required action:
 *   bit 0: match the end instead of the beginning (% or %%)
 *   bit 1: % or # was doubled so get the longest match
 *   bit 2: substring match
 *   bit 3: include the matched portion
 *   bit 4: include the unmatched portion
 *   bit 5: the index of the beginning
 *   bit 6: the index of the end
 *   bit 7: the length of the match
 *   bit 8: match the complete string
 * *sp points to the string we have to modify. The n'th match will be
 * returned in *sp. ncalloc is used to get memory for the result string.
 */

/**/
int
getmatch(char **sp, char *pat, int fl, int n)
{
    Comp c;
    char *s = *sp, *t, sav;
    int i, j, l = strlen(*sp);

    c = parsereg(pat);
    if (!c) {
	zerr("bad pattern: %s", pat, 0);
	return 1;
    }
    if (fl & 256) {
	i = domatch(s, c, 0);
	*sp = get_match_ret(*sp, 0, domatch(s, c, 0) ? l : 0, fl);
	if (! **sp && (((fl & 8) && !i) || ((fl & 16) && i)))
	    return 0;
	return 1;
    }
    switch (fl & 7) {
    case 0:
	/* Smallest possible match at head of string:    *
	 * start adding characters until we get a match. */
	for (i = 0, t = s; i <= l; i++, t++) {
	    sav = *t;
	    *t = '\0';
	    if (domatch(s, c, 0) && !--n) {
		*t = sav;
		*sp = get_match_ret(*sp, 0, i, fl);
		return 1;
	    }
	    if ((*t = sav) == Meta)
		i++, t++;
	}
	break;

    case 1:
	/* Smallest possible match at tail of string:  *
	 * move back down string until we get a match. */
	for (t = s + l; t >= s; t--) {
	    if (domatch(t, c, 0) && !--n) {
		*sp = get_match_ret(*sp, t - s, l, fl);
		return 1;
	    }
	    if (t > s+1 && t[-2] == Meta)
		t--;
	}
	break;

    case 2:
	/* Largest possible match at head of string:        *
	 * delete characters from end until we get a match. */
	for (t = s + l; t > s; t--) {
	    sav = *t;
	    *t = '\0';
	    if (domatch(s, c, 0) && !--n) {
		*t = sav;
		*sp = get_match_ret(*sp, 0, t - s, fl);
		return 1;
	    }
	    *t = sav;
	    if (t >= s+2 && t[-2] == Meta)
		t--;
	}
	break;

    case 3:
	/* Largest possible match at tail of string:       *
	 * move forward along string until we get a match. */
	for (i = 0, t = s; i < l; i++, t++) {
	    if (domatch(t, c, 0) && !--n) {
		*sp = get_match_ret(*sp, i, l, fl);
		return 1;
	    }
	    if (*t == Meta)
		i++, t++;
	}
	break;

    case 4:
	/* Smallest at start, but matching substrings. */
	if (domatch(s + l, c, 0) && !--n) {
	    *sp = get_match_ret(*sp, 0, 0, fl);
	    return 1;
	}
	for (i = 1; i <= l; i++) {
	    for (t = s, j = i; j <= l; j++, t++) {
		sav = s[j];
		s[j] = '\0';
		if (domatch(t, c, 0) && !--n) {
		    s[j] = sav;
		    *sp = get_match_ret(*sp, t - s, j, fl);
		    return 1;
		}
		if ((s[j] = sav) == Meta)
		    j++;
		if (*t == Meta)
		    t++;
	    }
	    if (s[i] == Meta)
		i++;
	}
	break;

    case 5:
	/* Smallest at end, matching substrings */
	if (domatch(s + l, c, 0) && !--n) {
	    *sp = get_match_ret(*sp, l, l, fl);
	    return 1;
	}
	for (i = l; i--;) {
	    if (i && s[i-1] == Meta)
		i--;
	    for (t = s + l, j = i; j >= 0; j--, t--) {
		sav = *t;
		*t = '\0';
		if (domatch(s + j, c, 0) && !--n) {
		    *t = sav;
		    *sp = get_match_ret(*sp, j, t - s, fl);
		    return 1;
		}
		*t = sav;
		if (t >= s+2 && t[-2] == Meta)
		    t--;
		if (j >= 2 && s[j-2] == Meta)
		    j--;
	    }
	}
	break;

    case 6:
	/* Largest at start, matching substrings. */
	for (i = l; i; i--) {
	    for (t = s, j = i; j <= l; j++, t++) {
		sav = s[j];
		s[j] = '\0';
		if (domatch(t, c, 0) && !--n) {
		    s[j] = sav;
		    *sp = get_match_ret(*sp, t - s, j, fl);
		    return 1;
		}
		if ((s[j] = sav) == Meta)
		    j++;
		if (*t == Meta)
		    t++;
	    }
	    if (i >= 2 && s[i-2] == Meta)
		i--;
	}
	if (domatch(s + l, c, 0) && !--n) {
	    *sp = get_match_ret(*sp, 0, 0, fl);
	    return 1;
	}
	break;

    case 7:
	/* Largest at end, matching substrings. */
	for (i = 0; i < l; i++) {
	    for (t = s + l, j = i; j >= 0; j--, t--) {
		sav = *t;
		*t = '\0';
		if (domatch(s + j, c, 0) && !--n) {
		    *t = sav;
		    *sp = get_match_ret(*sp, j, t - s, fl);
		    return 1;
		}
		*t = sav;
		if (t >= s+2 && t[-2] == Meta)
		    t--;
		if (j >= 2 && s[j-2] == Meta)
		    j--;
	    }
	    if (s[i] == Meta)
		i++;
	}
	if (domatch(s + l, c, 0) && !--n) {
	    *sp = get_match_ret(*sp, l, l, fl);
	    return 1;
	}
	break;
    }
    /* munge the whole string */
    *sp = get_match_ret(*sp, 0, 0, fl);
    return 1;
}

/* Add a component to pathbuf: This keeps track of how    *
 * far we are into a file name, since each path component *
 * must be matched separately.                            */

static int addpath _((char *s));

static int
addpath(char *s)
{
    if ((int)strlen(s) + pathpos + 2 >= PATH_MAX)
	return 0;
    while ((pathbuf[pathpos++] = *s++));
    pathbuf[pathpos - 1] = '/';
    pathbuf[pathpos] = '\0';
    return 1;
}

/* return full path for s, which has path as *
 * already added to pathbuf                  */

/**/
char *
getfullpath(char *s)
{
    static char buf[PATH_MAX];

    strcpy(buf, pathbuf);
    strncpy(buf + pathpos, s, PATH_MAX - pathpos - 1);
    return buf;
}

/* Do the globbing:  scanner is called recursively *
 * with successive bits of the path until we've    *
 * tried all of it.                                */

/**/
void
scanner(Complist q)
{
    Comp c;
    int closure;
    struct stat st;

    if (!q)
	return;

    /* make sure we haven't just done this one. */
    if (q->closure && old_pos != pathpos &&
	stat((*pathbuf) ? unmeta(pathbuf) : ".", &st) != -1) {
	if (st.st_ino == old_ino && st.st_dev == old_dev)
	    return;
	else {
	    old_pos = pathpos;
	    old_ino = st.st_ino;
	    old_dev = st.st_dev;
	}
    }
    if ((closure = q->closure)) { /* (foo/)# - match zero or more dirs */
	if (q->closure == 2)	  /* (foo/)## - match one or more dirs */
	    q->closure = 1;
	else
	    scanner(q->next);
    }
    if ((c = q->comp)) {
	/* Now the actual matching for the current path section. */
	if (!(c->next || c->left) && !haswilds(c->str)) {
	    /* It's a straight string to the end of the path section. */
	    if (q->next) {
		/* Not the last path section. Just add it to the path. */
		int oppos = pathpos;

		if (errflag)
		    return;
		if (q->closure && !strcmp(c->str, "."))
		    return;
		if (!addpath(c->str))
		    return;
		if (!closure || exists(pathbuf))
		    scanner((q->closure) ? q : q->next);
		pathbuf[pathpos = oppos] = '\0';
	    } else if (!*c->str) {
		if (exists(getfullpath(".")))
		    insert(dupstring(pathbuf));
	    } else {
		/* Last path section.  See if there's a file there. */
		char *s;

		if (exists(s = getfullpath(c->str)))
		    insert(dupstring(s));
	    }
	} else {
	    /* Do pattern matching on current path section. */
	    char *fn;
	    int dirs = !!q->next;
	    DIR *lock = opendir((*pathbuf) ? unmeta(pathbuf) : ".");

	    if (lock == NULL)
		return;
	    while ((fn = zreaddir(lock))) {
		/* Loop through the directory */
		if (errflag)
		    break;
		/* skip this and parent directory */
		if (fn[0] == '.'
		    && (fn[1] == '\0'
			|| (fn[1] == '.' && fn[2] == '\0')))
		    continue;
		/* prefix and suffix are zle trickery */
		if (!dirs && !colonmod &&
		    ((glob_pre && !strpfx(glob_pre, fn))
		     || (glob_suf && !strsfx(glob_suf, fn))))
		    continue;
		if (domatch(fn, c, gf_noglobdots)) {
		    /* if this name matchs the pattern... */
		    int oppos = pathpos;

		    if (dirs) {
			/* if not the last component in the path */
			if (closure) {
			    /* if matching multiple directories */
			    struct stat buf;

			    if ((q->follow ?
				stat(unmeta(getfullpath(fn)), &buf) :
				lstat(unmeta(getfullpath(fn)), &buf)) == -1) {
				if (errno != ENOENT && errno != EINTR &&
				    errno != ENOTDIR) {
				    zerr("%e: %s", fn, errno);
				    errflag = 0;
				}
				continue;
			    }
			    if (!S_ISDIR(buf.st_mode))
				continue;
			}
			/* do next path component */
			if (addpath(fn))
			    scanner((q->closure) ? q : q->next);	/* scan next level */
			pathbuf[pathpos = oppos] = '\0';
		    } else
			insert(dyncat(pathbuf, fn));
		    /* if the last filename component, just add it */
		}
	    }
	    closedir(lock);
	}
    } else
	zerr("no idea how you got this error message.", NULL, 0);
}

/* Flags passed down to guts when compiling */
#define GF_PATHADD	1	/* file glob, adding path components */
#define GF_TOPLEV	2	/* outside (), so ~ ends main match */

static char *pptr;		/* current place in string being matched */
static Comp tail;
static int first;		/* are leading dots special? */

/* The main entry point for matching a string str against  *
 * a compiled pattern c.  `fist' indicates whether leading *
 * dots are special.                                       */

/**/
int
domatch(char *str, Comp c, int fist)
{
    pptr = str;
    first = fist;
    if (*pptr == Nularg)
	pptr++;
    return doesmatch(c);
}

#define untok(C)  (itok(C) ? ztokens[(C) - Pound] : (C))

/* See if pattern has a matching exclusion (~...) part */

/**/
int
excluded(Comp c, char *eptr)
{
    char *saves = pptr;
    int savei = first, ret;

    first = 0;
    pptr = (PATHADDP(c) && pathpos) ? getfullpath(eptr) : eptr;

    ret = doesmatch(c->exclude);

    pptr = saves;
    first = savei;

    return ret;
}

/* see if current string in pptr matches c */

/**/
int
doesmatch(Comp c)
{
    char *pat = c->str;
    int done = 0;

  tailrec:
    if (ONEHASHP(c) || (done && TWOHASHP(c))) {
	/* Do multiple matches like (pat)# and (pat)## */
	char *saves = pptr;

	if (first && *pptr == '.')
	    return 0;
	if (doesmatch(c->next))
	    return 1;
	pptr = saves;
	first = 0;
    }
    done++;
    for (;;) {
	/* loop until success or failure of pattern */
	if (!pat || !*pat) {
	    /* No current pattern (c->str). */
	    char *saves;
	    int savei;

	    if (errflag)
		return 0;
	    /* Remember state in case we need to go back and   *
	     * check for exclusion of pattern or alternatives. */
	    saves = pptr;
	    savei = first;
	    /* Loop over alternatives with exclusions: (foo~bar|...). *
	     * Exclusions apply to the pattern in c->left.            */
	    if (c->left || c->right) {
		if (!doesmatch(c->left) ||
		    (c->exclude && excluded(c, saves))) {
		    if (c->right) {
			pptr = saves;
			first = savei;
			if (!doesmatch(c->right))
			    return 0;
		    } else
			return 0;
		}
	    }
	    if (*pptr && CLOSUREP(c)) {
		/* With a closure (#), need to keep trying */
		pat = c->str;
		goto tailrec;
	    }
	    if (!c->next)	/* no more patterns left */
		return (!LASTP(c) || !*pptr);
	    c = c->next;
	    done = 0;
	    pat = c->str;
	    goto tailrec;
	}
	/* Don't match leading dot if first is set */
	if (first && *pptr == '.' && *pat != '.')
	    return 0;
	if (*pat == Star) {	/* final * is not expanded to ?#; returns success */
	    while (*pptr)
		pptr++;
	    return 1;
	}
	first = 0;		/* finished checking start of pattern */
	if (*pat == Quest && *pptr) {
	    /* match exactly one character */
	    if (*pptr == Meta)
		pptr++;
	    pptr++;
	    pat++;
	    continue;
	}
	if (*pat == Hat)	/* following pattern is negated */
	    return 1 - doesmatch(c->next);
	if (*pat == Inbrack) {
	    /* Match groups of characters */
#define PAT(X) (pat[X] == Meta ? pat[(X)+1] ^ 32 : untok(pat[X]))
#define PPAT(X) (pat[(X)-1] == Meta ? pat[X] ^ 32 : untok(pat[X]))
	    char ch;
#if defined(HAVE_STRCOLL) && defined(ZSH_STRICT_POSIX)
	    char l_buf[2], r_buf[2], ch_buf[2];

	    l_buf[1] = r_buf[1] = ch_buf[1] = '\0';
#endif

	    if (!*pptr)
		break;
	    ch = *pptr == Meta ? pptr[1] ^ 32 : *pptr;
#if defined(HAVE_STRCOLL) && defined(ZSH_STRICT_POSIX)
	    ch_buf[0] = ch;
#endif
	    if (pat[1] == Hat || pat[1] == '^' || pat[1] == '!') {
		/* group is negated */
		pat[1] = Hat;
		for (pat += 2; *pat != Outbrack && *pat;
		     *pat == Meta ? pat += 2 : pat++)
		    if (*pat == '-' && pat[-1] != Hat && pat[1] != Outbrack) {
#if defined(HAVE_STRCOLL) && defined(ZSH_STRICT_POSIX)
			l_buf[0] = PPAT(-1);
			r_buf[0] = PAT(1);
			if (strcoll(l_buf, ch_buf) <= 0 &&
			    strcoll(ch_buf, r_buf) <= 0)
#else
			if (PPAT(-1) <= ch && PAT(1) >= ch)
#endif
			    break;
		    } else if (ch == PAT(0))
			break;
#ifdef DEBUG
		if (!*pat) {
		    zerr("something is very wrong.", NULL, 0);
		    return 0;
		}
#endif
		if (*pat != Outbrack)
		    break;
		pat++;
		*pptr == Meta ? pptr += 2 : pptr++;
		continue;
	    } else {
		/* pattern is not negated (affirmed? asserted?) */
		for (pat++; *pat != Outbrack && *pat;
		     *pat == Meta ? pat += 2 : pat++)
		    if (*pat == '-' && pat[-1] != Inbrack &&
			       pat[1] != Outbrack) {
#if defined(HAVE_STRCOLL) && defined(ZSH_STRICT_POSIX)
			l_buf[0] = PPAT(-1);
			r_buf[0] = PAT(1);
			if (strcoll(l_buf, ch_buf) <= 0 &&
			    strcoll(ch_buf, r_buf) <= 0)
#else
			if (PPAT(-1) <= ch && PAT(1) >= ch)
#endif
			    break;
		    } else if (ch == PAT(0))
			break;
#ifdef DEBUG
		if (!pat || !*pat) {
		    zerr("oh dear.  that CAN'T be right.", NULL, 0);
		    return 0;
		}
#endif
		if (*pat == Outbrack)
		    break;
		for (*pptr == Meta ? pptr += 2 : pptr++;
		     *pat != Outbrack; pat++);
		pat++;
		continue;
	    }
	}
	if (*pat == Inang) {
	    /* Numeric globbing. */
#ifdef ZSH_64_BIT_TYPE
/* zstrtol returns zlong anyway */
# define RANGE_CAST()
	    zlong t1, t2, t3;
#else
# define RANGE_CAST() (unsigned long)
	    unsigned long t1, t2, t3;
#endif
	    char *ptr;

	    if (!idigit(*pptr))
		break;
	    if (*++pat == Outang || 
		(*pat == '-' && pat[1] == Outang && ++pat)) {
		/* <> or <->:  any number matches */
		while (idigit(*++pptr));
		pat++;
	    } else {
		/* Flag that there is no upper limit */
		int not3 = 0;
		char *opptr = pptr;
		/*
		 * Form is <a-b>, where a or b are numbers or blank.
		 * t1 = number supplied:  must be positive, so use
		 * unsigned arithmetic.
		 */
		t1 = RANGE_CAST() zstrtol(pptr, &ptr, 10);
		pptr = ptr;
		/* t2 = lower limit */
		if (idigit(*pat))
		    t2 = RANGE_CAST() zstrtol(pat, &ptr, 10);
		else
		    t2 = 0, ptr = pat;
		if (*ptr != '-' || (not3 = (ptr[1] == Outang)))
				/* exact match or no upper limit */
		    t3 = t2, pat = ptr + not3;
		else		/* t3 = upper limit */
		    t3 = RANGE_CAST() zstrtol(ptr + 1, &pat, 10);
		DPUTS(*pat != Outang, "BUG: wrong internal range pattern");
		pat++;
		/*
		 * If the number found is too large for the pattern,
		 * try matching just the first part.  This way
		 * we always get the longest possible match.
		 */
		while (!not3 && t1 > t3 && pptr > opptr+1) {
		  pptr--;
		  t1 /= 10;
		}
		if (t1 < t2 || (!not3 && t1 > t3))
		    break;
	    }
	    continue;
#undef RANGE_CAST
	}
	if (*pptr == *pat) {
	    /* just plain old characters */
	    pptr++;
	    pat++;
	    continue;
	}
	break;
    }
    return 0;
}

/* turn a string into a Complist struct:  this has path components */

/**/
Complist
parsepat(char *str)
{
    mode = 0;			/* path components present */
    pptr = str;
    tail = NULL;
    return parsecomplist();
}

/* turn a string into a Comp struct:  this doesn't treat / specially */

/**/
Comp
parsereg(char *str)
{
    remnulargs(str);
    mode = 1;			/* no path components */
    pptr = str;
    tail = NULL;
    return parsecompsw(GF_TOPLEV);
}

/* Parse a series of path components pointed to by pptr */

/* This function tokenizes a zsh glob pattern */

/**/
Complist
parsecomplist(void)
{
    Comp c1;
    Complist p1;
    char *str;

    if (pptr[0] == Star && pptr[1] == Star &&
        (pptr[2] == '/' || (pptr[2] == Star && pptr[3] == '/'))) {
	/* Match any number of directories. */
	int follow;

	/* with three stars, follow symbolic links */
	follow = (pptr[2] == Star);
	pptr += (3 + follow);

	/* Now get the next path component if there is one. */
	p1 = (Complist) alloc(sizeof *p1);
	if ((p1->next = parsecomplist()) == NULL) {
	    errflag = 1;
	    return NULL;
	}
	p1->comp = (Comp) alloc(sizeof *p1->comp);
	p1->comp->stat |= C_LAST;	/* end of path component  */
	p1->comp->str = dupstring("*");
	*p1->comp->str = Star;		/* match anything...      */
	p1->closure = 1;		/* ...zero or more times. */
	p1->follow = follow;
	return p1;
    }

    /* Parse repeated directories such as (dir/)# and (dir/)## */
    if (*(str = pptr) == Inpar && !skipparens(Inpar, Outpar, &str) &&
        *str == Pound && isset(EXTENDEDGLOB) && str[-2] == '/') {
	pptr++;
	if (!(c1 = parsecompsw(0)))
	    return NULL;
	if (pptr[0] == '/' && pptr[1] == Outpar && pptr[2] == Pound) {
	    int pdflag = 0;

	    pptr += 3;
	    if (*pptr == Pound) {
		pdflag = 1;
		pptr++;
	    }
	    p1 = (Complist) alloc(sizeof *p1);
	    p1->comp = c1;
	    p1->closure = 1 + pdflag;
	    p1->follow = 0;
	    p1->next = parsecomplist();
	    return (p1->comp) ? p1 : NULL;
	}
    } else {
	/* parse single path component */
	if (!(c1 = parsecompsw(GF_PATHADD|GF_TOPLEV)))
	    return NULL;
	/* then do the remaining path compoents */
	if (*pptr == '/' || !*pptr) {
	    int ef = *pptr == '/';

	    p1 = (Complist) alloc(sizeof *p1);
	    p1->comp = c1;
	    p1->closure = 0;
	    p1->next = ef ? (pptr++, parsecomplist()) : NULL;
	    return (ef && !p1->next) ? NULL : p1;
	}
    }
    errflag = 1;
    return NULL;
}

/* parse lowest level pattern */

/**/
Comp
parsecomp(int gflag)
{
    Comp c = (Comp) alloc(sizeof *c), c1, c2;
    char *cstr, *ls = NULL;

    /* In case of alternatives, code coming up is stored in tail. */
    c->next = tail;
    cstr = pptr;

    while (*pptr && (mode || *pptr != '/') && *pptr != Bar &&
	   (unset(EXTENDEDGLOB) || *pptr != Tilde ||
	    !pptr[1] || pptr[1] == Outpar || pptr[1] == Bar) &&
	   *pptr != Outpar) {
	/* Go through code until we find something separating alternatives,
	 * or path components if relevant.
	 */
	if (*pptr == Hat && isset(EXTENDEDGLOB)) {
	    /* negate remaining pattern */
	    pptr++;
	    c->str = dupstrpfx(cstr, pptr - cstr);
	    if (!(c->next = parsecomp(gflag)))
		return NULL;
	    return c;
	}
	if (*pptr == Star && pptr[1] &&
	    (unset(EXTENDEDGLOB) || pptr[1] != Tilde || !pptr[2] ||
	     pptr[2] == Bar ||
	     pptr[2] == Outpar) && (mode || pptr[1] != '/')) {
	    /* Star followed by other patterns is treated like a closure
	     * (zero or more repetitions) of the single character pattern
	     * operator `?'.
	     */
	    c->str = dupstrpfx(cstr, pptr - cstr);
	    pptr++;
	    c1 = (Comp) alloc(sizeof *c1);
	    *(c1->str = dupstring("?")) = Quest;
	    c1->stat |= C_ONEHASH;
	    if (!(c2 = parsecomp(gflag)))
		return NULL;
	    c1->next = c2;
	    c->next = c1;
	    return c;
	}
	if (*pptr == Inpar) {
	    /* Found a group (...) */
	    char *startp = pptr, *endp;
	    Comp stail = tail;
	    int dpnd = 0;

	    /* Need matching close parenthesis */
	    if (skipparens(Inpar, Outpar, &pptr)) {
		errflag = 1;
		return NULL;
	    }
	    if (*pptr == Pound && isset(EXTENDEDGLOB)) {
		/* Zero (or one) or more repetitions of group */
		dpnd = 1;
		pptr++;
		if (*pptr == Pound) {
		    pptr++;
		    dpnd = 2;
		}
	    }
	    /* Parse the remaining pattern following the group... */
	    if (!(c1 = parsecomp(gflag)))
		return NULL;
	    /* ...remembering what comes after it... */
	    tail = dpnd ? NULL : c1;
	    /* ...before going back and parsing inside the group. */
	    endp = pptr;
	    pptr = startp;
	    c->str = dupstrpfx(cstr, pptr - cstr);
	    pptr++;
	    c->next = (Comp) alloc(sizeof *c);
	    if (!(c->next->left = parsecompsw(0)))
		return NULL;
	    /* Remember closures for group. */
	    if (dpnd)
		c->next->stat |= (dpnd == 2) ? C_TWOHASH : C_ONEHASH;
	    c->next->next = dpnd ? c1 : (Comp) alloc(sizeof *c);
	    pptr = endp;
	    tail = stail;
	    return c;
	}
	if (*pptr == Pound && isset(EXTENDEDGLOB)) {
	    /* repeat whatever we've just had (ls) zero or more times */
	    if (!ls)
		return NULL;
	    c2 = (Comp) alloc(sizeof *c);
	    c2->str = dupstrpfx(ls, pptr - ls);
	    pptr++;
	    if (*pptr == Pound) {
		/* need one or more matches: cheat by copying previous char */
		pptr++;
		c->next = c1 = (Comp) alloc(sizeof *c);
		c1->str = c2->str;
	    } else
		c1 = c;
	    c1->next = c2;
	    c2->stat |= C_ONEHASH;
	    /* parse the rest of the pattern and return. */
	    c2->next = parsecomp(gflag);
	    if (!c2->next)
		return NULL;
	    c->str = dupstrpfx(cstr, ls - cstr);
	    return c;
	}
	ls = pptr;		/* whatever we just parsed */
	if (*pptr == Inang) {
	    /* Numeric glob */
	    int dshct;

	    dshct = (pptr[1] == Outang);
	    while (*++pptr && *pptr != Outang)
		if (*pptr == '-' && !dshct)
		    dshct = 1;
		else if (!idigit(*pptr))
		    break;
	    if (*pptr != Outang)
		return NULL;
	} else if (*pptr == Inbrack) {
	    /* Character set: brackets had better match */
	    while (*++pptr && *pptr != Outbrack)
		if (itok(*pptr))
		    *pptr = ztokens[*pptr - Pound];
	    if (*pptr != Outbrack)
		return NULL;
	} else if (itok(*pptr) && *pptr != Star && *pptr != Quest)
	    /* something that can be tokenised which isn't otherwise special */
	    *pptr = ztokens[*pptr - Pound];
	pptr++;
    }
    /* mark if last pattern component in path component or pattern */
    if (*pptr == '/' || !*pptr ||
	(isset(EXTENDEDGLOB) && *pptr == Tilde && (gflag & GF_TOPLEV)))
	c->stat |= C_LAST;
    c->str = dupstrpfx(cstr, pptr - cstr);
    return c;
}

/* Parse pattern possibly with different alternatives (|) */

/**/
Comp
parsecompsw(int gflag)
{
    Comp c1, c2, c3, excl = NULL;

    c1 = parsecomp(gflag);
    if (!c1)
	return NULL;
    if (isset(EXTENDEDGLOB) && *pptr == Tilde) {
	/* Matching remainder of pattern excludes the pattern from matching */
	int oldmode = mode;

	mode = 1;
	pptr++;
	excl = parsecomp(gflag);
	mode = oldmode;
	if (!excl)
	    return NULL;
    }
    if (*pptr == Bar || excl) {
	/* found an alternative or something to exclude */
	c2 = (Comp) alloc(sizeof *c2);
	if (*pptr == Bar) {
	    /* get the next alternative after the | */
	    pptr++;
	    c3 = parsecompsw(gflag);
	    if (!c3)
		return NULL;
	} else {
	    /* mark if end of pattern or path component */
	    if (!*pptr || *pptr == '/')
		c2->stat |= C_LAST;
	    c3 = NULL;
	}
	c2->str = dupstring("");
	c2->left = c1;
	c2->right = c3;
	c2->exclude = excl;
	if (gflag & GF_PATHADD)
	    c2->stat |= C_PATHADD;
	return c2;
    }
    return c1;
}

/* blindly turn a string into a tokenised expression without lexing */

/**/
void
tokenize(char *s)
{
    char *t;
    int bslash = 0;

    for (; *s; s++) {
      cont:
	switch (*s) {
	case Bnull:
	case '\\':
	    if (bslash) {
		s[-1] = Bnull;
		break;
	    }
	    bslash = 1;
	    continue;
	case '[':
	    if (bslash) {
		s[-1] = Bnull;
		break;
	    }
	    t = s;
	    if (*++s == '^' || *s == '!')
		s++;
	    while (*s && *++s != ']');
	    if (!*s)
		return;
	    *t = Inbrack;
	    *s = Outbrack;
	    break;
	case '<':
	    if (isset(SHGLOB))
		break;
	    if (bslash) {
		s[-1] = Bnull;
		break;
	    }
	    t = s;
	    while (idigit(*++s));
	    if (*s != '-')
		goto cont;
	    while (idigit(*++s));
	    if (*s != '>')
		goto cont;
	    *t = Inang;
	    *s = Outang;
	    break;
	case '(':
	case '|':
	case ')':
	    if (isset(SHGLOB))
		break;
	case '^':
	case '#':
	case '~':
	case '*':
	case '?':
	    for (t = ztokens; *t; t++)
		if (*t == *s) {
		    if (bslash)
			s[-1] = Bnull;
		    else
			*s = (t - ztokens) + Pound;
		    break;
		}
	}
	bslash = 0;
    }
}

/* remove unnecessary Nulargs */

/**/
void
remnulargs(char *s)
{
    int nl = *s;
    char *t = s;

    while (*s)
	if (INULL(*s))
	    chuck(s);
	else
	    s++;
    if (!*t && nl) {
	t[0] = Nularg;
	t[1] = '\0';
    }
}

/* qualifier functions:  mostly self-explanatory, see glob(). */

/* device number */

/**/
int
qualdev(struct stat *buf, off_t dv)
{
    return buf->st_dev == dv;
}

/* number of hard links to file */

/**/
int
qualnlink(struct stat *buf, off_t ct)
{
    return (range < 0 ? buf->st_nlink < ct :
	    range > 0 ? buf->st_nlink > ct :
	    buf->st_nlink == ct);
}

/* user ID */

/**/
int
qualuid(struct stat *buf, off_t uid)
{
    return buf->st_uid == uid;
}

/* group ID */

/**/
int
qualgid(struct stat *buf, off_t gid)
{
    return buf->st_gid == gid;
}

/* device special file? */

/**/
int
qualisdev(struct stat *buf, off_t junk)
{
    junk = buf->st_mode & S_IFMT;
    return junk == S_IFBLK || junk == S_IFCHR;
}

/* block special file? */

/**/
int
qualisblk(struct stat *buf, off_t junk)
{
    junk = buf->st_mode & S_IFMT;
    return junk == S_IFBLK;
}

/* character special file? */

/**/
int
qualischar(struct stat *buf, off_t junk)
{
    junk = buf->st_mode & S_IFMT;
    return junk == S_IFCHR;
}

/* file type is requested one */

/**/
int
qualmode(struct stat *buf, off_t mod)
{
    return (buf->st_mode & S_IFMT) == mod;
}

/* given flag is set in mode */

/**/
int
qualflags(struct stat *buf, off_t mod)
{
    return buf->st_mode & mod;
}

/* mode matches number supplied exactly  */

/**/
int
qualeqflags(struct stat *buf, off_t mod)
{
    return (buf->st_mode & 07777) == mod;
}

/* regular executable file? */

/**/
int
qualiscom(struct stat *buf, off_t mod)
{
    return (buf->st_mode & (S_IFMT | S_IEXEC)) == (S_IFREG | S_IEXEC);
}

/* size in required range? */

/**/
int
qualsize(struct stat *buf, off_t size)
{
#if defined(LONG_IS_64_BIT) || defined(OFF_T_IS_64_BIT)
# define QS_CAST_SIZE()
    off_t scaled = buf->st_size;
#else
# define QS_CAST_SIZE() (unsigned long)
    unsigned long scaled = (unsigned long)buf->st_size;
#endif

    switch (units) {
    case TT_POSIX_BLOCKS:
	scaled += 511l;
	scaled /= 512l;
	break;
    case TT_KILOBYTES:
	scaled += 1023l;
	scaled /= 1024l;
	break;
    case TT_MEGABYTES:
	scaled += 1048575l;
	scaled /= 1048576l;
	break;
    }

    return (range < 0 ? scaled < QS_CAST_SIZE() size :
	    range > 0 ? scaled > QS_CAST_SIZE() size :
	    scaled == QS_CAST_SIZE() size);
#undef QS_CAST_SIZE
}

/* time in required range? */

/**/
int
qualtime(struct stat *buf, off_t days)
{
    time_t now, diff;

    time(&now);
    diff = now - (amc == 0 ? buf->st_atime : amc == 1 ? buf->st_mtime :
		  buf->st_ctime);
    /* handle multipliers indicating units */
    switch (units) {
    case TT_DAYS:
	diff /= 86400l;
	break;
    case TT_HOURS:
	diff /= 3600l;
	break;
    case TT_MINS:
	diff /= 60l;
	break;
    case TT_WEEKS:
	diff /= 604800l;
	break;
    case TT_MONTHS:
	diff /= 2592000l;
	break;
    }

    return (range < 0 ? diff < days :
	    range > 0 ? diff > days :
	    diff == days);
}
