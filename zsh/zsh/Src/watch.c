/*
 * watch.c - login/logout watching
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

static int wtabsz;
STRUCT_UTMP *wtab;
static time_t lastutmpcheck;

/* get the time of login/logout for WATCH */

/**/
time_t
getlogtime(STRUCT_UTMP *u, int inout)
{
    FILE *in;
    STRUCT_UTMP uu;
    int first = 1;
    int srchlimit = 50;		/* max number of wtmp records to search */

    if (inout)
	return u->ut_time;
    if (!(in = fopen(WTMP_FILE, "r")))
	return time(NULL);
    fseek(in, 0, 2);
    do {
	if (fseek(in, ((first) ? -1 : -2) * sizeof(STRUCT_UTMP), 1)) {
	    fclose(in);
	    return time(NULL);
	}
	first = 0;
	if (!fread(&uu, sizeof(STRUCT_UTMP), 1, in)) {
	    fclose(in);
	    return time(NULL);
	}
	if (uu.ut_time < lastwatch || !srchlimit--) {
	    fclose(in);
	    return time(NULL);
	}
    }
    while (memcmp(&uu, u, sizeof(uu)));

    do
	if (!fread(&uu, sizeof(STRUCT_UTMP), 1, in)) {
	    fclose(in);
	    return time(NULL);
	}
    while (strncmp(uu.ut_line, u->ut_line, sizeof(u->ut_line)));
    fclose(in);
    return uu.ut_time;
}

/* Mutually recursive call to handle ternaries in $WATCHFMT */

#define BEGIN3	'('
#define END3	')'

/**/
char *
watch3ary(int inout, STRUCT_UTMP *u, char *fmt, int prnt)
{
    int truth = 1, sep;

    switch (*fmt++) {
    case 'n':
	truth = (u->ut_name[0] != 0);
	break;
    case 'a':
	truth = inout;
	break;
    case 'l':
	if (!strncmp(u->ut_line, "tty", 3))
	    truth = (u->ut_line[3] != 0);
	else
	    truth = (u->ut_line[0] != 0);
	break;
#ifdef HAVE_UT_HOST
    case 'm':
    case 'M':
	truth = (u->ut_host[0] != 0);
	break;
#endif
    default:
	prnt = 0;		/* Skip unknown conditionals entirely */
	break;
    }
    sep = *fmt++;
    fmt = watchlog2(inout, u, fmt, (truth && prnt), sep);
    return watchlog2(inout, u, fmt, (!truth && prnt), END3);
}

/* print a login/logout event */

/**/
char *
watchlog2(int inout, STRUCT_UTMP *u, char *fmt, int prnt, int fini)
{
    char buf[40], buf2[80];
    time_t timet;
    struct tm *tm;
    char *fm2;
#ifdef HAVE_UT_HOST
    char *p;
    int i;
#endif

    while (*fmt)
	if (*fmt == '\\') {
	    if (*++fmt) {
		if (prnt)
		    putchar(*fmt);
		++fmt;
	    } else if (fini)
		return fmt;
	    else
		break;
	} else if (*fmt == fini)
	    return ++fmt;
	else if (*fmt != '%') {
	    if (prnt)
		putchar(*fmt);
	    ++fmt;
	} else {
	    if (*++fmt == BEGIN3)
		fmt = watch3ary(inout, u, ++fmt, prnt);
	    else if (!prnt)
		++fmt;
	    else
		switch (*(fm2 = fmt++)) {
		case 'n':
		    printf("%.*s", (int)sizeof(u->ut_name), u->ut_name);
		    break;
		case 'a':
		    printf("%s", (!inout) ? "logged off" : "logged on");
		    break;
		case 'l':
		    if (!strncmp(u->ut_line, "tty", 3))
			printf("%.*s", (int)sizeof(u->ut_line) - 3, u->ut_line + 3);
		    else
			printf("%.*s", (int)sizeof(u->ut_line), u->ut_line);
		    break;
#ifdef HAVE_UT_HOST
		case 'm':
		    for (p = u->ut_host, i = sizeof(u->ut_host); i && *p; i--, p++) {
			if (*p == '.' && !idigit(p[1]))
			    break;
			putchar(*p);
		    }
		    break;
		case 'M':
		    printf("%.*s", (int)sizeof(u->ut_host), u->ut_host);
		    break;
#endif
		case 'T':
		case 't':
		case '@':
		case 'W':
		case 'w':
		case 'D':
		    switch (*fm2) {
		    case '@':
		    case 't':
			fm2 = "%l:%M%p";
			break;
		    case 'T':
			fm2 = "%k:%M";
			break;
		    case 'w':
			fm2 = "%a %e";
			break;
		    case 'W':
			fm2 = "%m/%d/%y";
			break;
		    case 'D':
			if (fm2[1] == '{') {
			    char *dd, *ss;
			    int n = 79;

			    for (ss = fm2 + 2, dd = buf2;
				 n-- && *ss && *ss != '}'; ++ss, ++dd)
				*dd = *((*ss == '\\' && ss[1]) ? ++ss : ss);
			    if (*ss == '}') {
				*dd = '\0';
				fmt = ss + 1;
				fm2 = buf2;
			    }
			    else fm2 = "%y-%m-%d";
			}
			else fm2 = "%y-%m-%d";
			break;
		    }
		    timet = getlogtime(u, inout);
		    tm = localtime(&timet);
		    ztrftime(buf, 40, fm2, tm);
		    printf("%s", (*buf == ' ') ? buf + 1 : buf);
		    break;
		case '%':
		    putchar('%');
		    break;
		case 'S':
		    txtset(TXTSTANDOUT);
		    tsetcap(TCSTANDOUTBEG, -1);
		    break;
		case 's':
		    txtset(TXTDIRTY);
		    txtunset(TXTSTANDOUT);
		    tsetcap(TCSTANDOUTEND, -1);
		    break;
		case 'B':
		    txtset(TXTDIRTY);
		    txtset(TXTBOLDFACE);
		    tsetcap(TCBOLDFACEBEG, -1);
		    break;
		case 'b':
		    txtset(TXTDIRTY);
		    txtunset(TXTBOLDFACE);
		    tsetcap(TCALLATTRSOFF, -1);
		    break;
		case 'U':
		    txtset(TXTUNDERLINE);
		    tsetcap(TCUNDERLINEBEG, -1);
		    break;
		case 'u':
		    txtset(TXTDIRTY);
		    txtunset(TXTUNDERLINE);
		    tsetcap(TCUNDERLINEEND, -1);
		    break;
		default:
		    putchar('%');
		    putchar(*fm2);
		    break;
		}
	}
    if (prnt)
	putchar('\n');

    return fmt;
}

/* check the List for login/logouts */

/**/
void
watchlog(int inout, STRUCT_UTMP *u, char **w, char *fmt)
{
    char *v, *vv, sav;
    int bad;

    if (!*u->ut_name)
	return;

    if (*w && !strcmp(*w, "all")) {
	(void)watchlog2(inout, u, fmt, 1, 0);
	return;
    }
    if (*w && !strcmp(*w, "notme") &&
	strncmp(u->ut_name, get_username(), sizeof(u->ut_name))) {
	(void)watchlog2(inout, u, fmt, 1, 0);
	return;
    }
    for (; *w; w++) {
	bad = 0;
	v = *w;
	if (*v != '@' && *v != '%') {
	    for (vv = v; *vv && *vv != '@' && *vv != '%'; vv++);
	    sav = *vv;
	    *vv = '\0';
	    if (strncmp(u->ut_name, v, sizeof(u->ut_name)))
		bad = 1;
	    *vv = sav;
	    v = vv;
	}
	for (;;)
	    if (*v == '%') {
		for (vv = ++v; *vv && *vv != '@'; vv++);
		sav = *vv;
		*vv = '\0';
		if (strncmp(u->ut_line, v, sizeof(u->ut_line)))
		    bad = 1;
		*vv = sav;
		v = vv;
	    }
#ifdef HAVE_UT_HOST
	    else if (*v == '@') {
		for (vv = ++v; *vv && *vv != '%'; vv++);
		sav = *vv;
		*vv = '\0';
		if (strncmp(u->ut_host, v, strlen(v)))
		    bad = 1;
		*vv = sav;
		v = vv;
	    }
#endif
	    else
		break;
	if (!bad) {
	    (void)watchlog2(inout, u, fmt, 1, 0);
	    return;
	}
    }
}

/* compare 2 utmp entries */

/**/
int
ucmp(STRUCT_UTMP *u, STRUCT_UTMP *v)
{
    if (u->ut_time == v->ut_time)
	return strncmp(u->ut_line, v->ut_line, sizeof(u->ut_line));
    return u->ut_time - v->ut_time;
}

/* initialize the user List */

/**/
void
readwtab(void)
{
    STRUCT_UTMP *uptr;
    int wtabmax = 32;
    FILE *in;

    wtabsz = 0;
    if (!(in = fopen(UTMP_FILE, "r")))
	return;
    uptr = wtab = (STRUCT_UTMP *)zalloc(wtabmax * sizeof(STRUCT_UTMP));
    while (fread(uptr, sizeof(STRUCT_UTMP), 1, in))
#ifdef USER_PROCESS
	if   (uptr->ut_type == USER_PROCESS)
#else
	if   (uptr->ut_name[0])
#endif
	{
	    uptr++;
	    if (++wtabsz == wtabmax)
		uptr = (wtab = (STRUCT_UTMP *)realloc((void *) wtab, (wtabmax *= 2) *
						      sizeof(STRUCT_UTMP))) + wtabsz;
	}
    fclose(in);

    if (wtabsz)
	qsort((void *) wtab, wtabsz, sizeof(STRUCT_UTMP),
	           (int (*) _((const void *, const void *)))ucmp);
}

/* Check for login/logout events; executed before *
 * each prompt if WATCH is set                    */

/**/
void
dowatch(void)
{
    FILE *in;
    STRUCT_UTMP *utab, *uptr, *wptr;
    struct stat st;
    char **s;
    char *fmt;
    int utabsz = 0, utabmax = wtabsz + 4;
    int uct, wct;

    s = watch;
    if (!(fmt = getsparam("WATCHFMT")))
	fmt = DEFAULT_WATCHFMT;

    holdintr();
    if (!wtab) {
	readwtab();
	noholdintr();
	return;
    }
    if ((stat(UTMP_FILE, &st) == -1) || (st.st_mtime <= lastutmpcheck)) {
	noholdintr();
	return;
    }
    lastutmpcheck = st.st_mtime;
    uptr = utab = (STRUCT_UTMP *) zalloc(utabmax * sizeof(STRUCT_UTMP));

    if (!(in = fopen(UTMP_FILE, "r"))) {
	free(utab);
	noholdintr();
	return;
    }
    while (fread(uptr, sizeof *uptr, 1, in))
#ifdef USER_PROCESS
	if (uptr->ut_type == USER_PROCESS)
#else
	if (uptr->ut_name[0])
#endif
	{
	    uptr++;
	    if (++utabsz == utabmax)
		uptr = (utab = (STRUCT_UTMP *)realloc((void *) utab, (utabmax *= 2) *
						      sizeof(STRUCT_UTMP))) + utabsz;
	}
    fclose(in);
    noholdintr();
    if (errflag) {
	free(utab);
	return;
    }
    if (utabsz)
	qsort((void *) utab, utabsz, sizeof(STRUCT_UTMP),
	           (int (*) _((const void *, const void *)))ucmp);

    wct = wtabsz;
    uct = utabsz;
    uptr = utab;
    wptr = wtab;
    if (errflag) {
	free(utab);
	return;
    }
    while ((uct || wct) && !errflag)
	if (!uct || (wct && ucmp(uptr, wptr) > 0))
	    wct--, watchlog(0, wptr++, s, fmt);
	else if (!wct || (uct && ucmp(uptr, wptr) < 0))
	    uct--, watchlog(1, uptr++, s, fmt);
	else
	    uptr++, wptr++, wct--, uct--;
    free(wtab);
    wtab = utab;
    wtabsz = utabsz;
    fflush(stdout);
}

/**/
int
bin_log(char *nam, char **argv, char *ops, int func)
{
    if (!watch)
	return 1;
    if (wtab)
	free(wtab);
    wtab = (STRUCT_UTMP *)zalloc(1);
    wtabsz = 0;
    lastutmpcheck = 0;
    dowatch();
    return 0;
}

