/*
 * env.c -- small service routines
 *
 * Copyright 1998 by Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 */

#include "config.h"
#include <stdio.h>
#include <ctype.h>
#if defined(STDC_HEADERS)
#include <stdlib.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include <pwd.h>
#include <string.h>
#ifdef HAVE_NET_SOCKET_H
#include <net/socket.h>
#endif
#ifdef HAVE_GETHOSTBYNAME
#include <netdb.h>
#endif /* HAVE_GETHOSTBYNAME */
#include  <sys/types.h>
#include  <time.h>
#include "fetchmail.h"

#include "i18n.h"
#if defined(HAVE_SETLOCALE) && defined(ENABLE_NLS) && defined(HAVE_STRFTIME)
#include <time.h>
#include <locale.h>
#endif

extern char *getenv();	/* needed on sysV68 R3V7.1. */

extern char *program_name;

void envquery(int argc, char **argv)
/* set up basic stuff from the environment (including the rc file name) */
{
    struct passwd by_name, by_uid, *pwp;

    if (!(user = getenv("FETCHMAILUSER")))
    {
	if (!(user = getenv("LOGNAME")))
	{
	    user = getenv("USER");
	}
    }

    if (!(pwp = getpwuid(getuid())))
    {
	fprintf(stderr,
		_("%s: You don't exist.  Go away.\n"),
		program_name);
	exit(PS_UNDEFINED);
    }
    else
    {
	memcpy(&by_uid, pwp, sizeof(struct passwd));
	if (!user || !(pwp = getpwnam(user)))
	    pwp = &by_uid;
	else
	{
	    /*
	     * This logic is needed to handle gracefully the possibility
	     * that multiple names might be mapped to one UID.
	     */
	    memcpy(&by_name, pwp, sizeof(struct passwd));

	    if (by_name.pw_uid == by_uid.pw_uid)
		pwp = &by_name;
	    else
		pwp = &by_uid;
	}
	user = xstrdup(pwp->pw_name);
    }

    /* compute user's home directory */
    if (!(home = getenv("HOME")))
	home = pwp->pw_dir;

    /* compute fetchmail's home directory */
    if (!(fmhome = getenv("FETCHMAILHOME")))
	fmhome = home;

    if ((program_name = strrchr(argv[0], '/')) != NULL)
	++program_name;
    else
	program_name = argv[0];

#define RCFILE_NAME	"fetchmailrc"
    /*
     * The (fmhome==home) leaves an extra character for a . at the
     * beginning of the rc file's name, iff fetchmail is using $HOME
     * for its files. We don't want to do that if fetchmail has its
     * own home ($FETCHMAILHOME), however.
     */
    rcfile = (char *)xmalloc(strlen(fmhome)+sizeof(RCFILE_NAME)+(fmhome==home)+2);
    /* avoid //.fetchmailrc */
    if (strcmp(fmhome, "/") != 0)
	strcpy(rcfile, fmhome);
    else
	*rcfile = '\0';

    if (rcfile[strlen(rcfile) - 1] != '/')
	strcat(rcfile, "/");
    if (fmhome==home)
	strcat(rcfile, ".");
    strcat(rcfile, RCFILE_NAME);
}

char *host_fqdn(void)
/* get the FQDN of the machine we're running */
{
    char	tmpbuf[HOSTLEN+1];

    if (gethostname(tmpbuf, sizeof(tmpbuf)))
    {
	fprintf(stderr, _("%s: can't determine your host!"),
		program_name);
	exit(PS_DNS);
    }
#ifdef HAVE_GETHOSTBYNAME
    /* if we got a . in the hostname assume it is a FQDN */
    if (strchr(tmpbuf, '.') == NULL)
    {
	struct hostent *hp;

	/* if we got a basename (as we do in Linux) make a FQDN of it */
	hp = gethostbyname(tmpbuf);
	if (hp == (struct hostent *) NULL)
	{
	    /* exit with error message */
	    fprintf(stderr,
		    _("gethostbyname failed for %s\n"), tmpbuf);
	    exit(PS_DNS);
	}
	return(xstrdup(hp->h_name));
    }
    else
#endif /* HAVE_GETHOSTBYNAME */
	return(xstrdup(tmpbuf));
}

static char *tzoffset(time_t *now)
/* calculate timezone offset */
{
    static char offset_string[6];
    struct tm gmt, *lt;
    int off;
    char sign = '+';

    gmt = *gmtime(now);
    lt = localtime(now);
    off = (lt->tm_hour - gmt.tm_hour) * 60 + lt->tm_min - gmt.tm_min;
    if (lt->tm_year < gmt.tm_year)
	off -= 24 * 60;
    else if (lt->tm_year > gmt.tm_year)
	off += 24 * 60;
    else if (lt->tm_yday < gmt.tm_yday)
	off -= 24 * 60;
    else if (lt->tm_yday > gmt.tm_yday)
	off += 24 * 60;
    if (off < 0) {
	sign = '-';
	off = -off;
    }
    if (off >= 24 * 60)			/* should be impossible */
	off = 23 * 60 + 59;		/* if not, insert silly value */
    sprintf(offset_string, "%c%02d%02d", sign, off / 60, off % 60);
    return (offset_string);
}

char *rfc822timestamp(void)
/* return a timestamp in RFC822 form */
{
    time_t	now;
    static char buf[50];

    time(&now);
#ifdef HAVE_STRFTIME
    /*
     * Conform to RFC822.  We generate a 4-digit year here, avoiding
     * Y2K hassles.  Max length of this timestamp in an English locale
     * should be 29 chars.  The only things that should vary by locale
     * are the day and month abbreviations.  The set_locale calls prevent
     * weird multibyte i18n characters (such as kanji) for showing up
     * in your Received headers.
     */
#if defined(HAVE_SETLOCALE) && defined(ENABLE_NLS) && defined(HAVE_STRFTIME)
    setlocale (LC_TIME, "C");
#endif
    strftime(buf, sizeof(buf)-1, 
	     "%a, %d %b %Y %H:%M:%S XXXXX (%Z)", localtime(&now));
#if defined(HAVE_SETLOCALE) && defined(ENABLE_NLS) && defined(HAVE_STRFTIME)
    setlocale (LC_TIME, "");
#endif
    strncpy(strstr(buf, "XXXXX"), tzoffset(&now), 5);
#else
    /*
     * This is really just a portability fallback, as the
     * date format ctime(3) emits is not RFC822
     * conformant.
     */
    strcpy(buf, ctime(&now));
    buf[strlen(buf)-1] = '\0';	/* remove trailing \n */
#endif /* HAVE_STRFTIME */

    return(buf);
}

const char *showproto(int proto)
/* protocol index to protocol name mapping */
{
    switch (proto)
    {
    case P_AUTO: return("auto");
#ifdef POP2_ENABLE
    case P_POP2: return("POP2");
#endif /* POP2_ENABLE */
#ifdef POP3_ENABLE
    case P_POP3: return("POP3");
    case P_APOP: return("APOP");
    case P_RPOP: return("RPOP");
#endif /* POP3_ENABLE */
#ifdef IMAP_ENABLE
    case P_IMAP: return("IMAP");
#endif /* IMAP_ENABLE */
#ifdef ETRN_ENABLE
    case P_ETRN: return("ETRN");
#endif /* ETRN_ENABLE */
#ifdef ODMR_ENABLE
    case P_ODMR: return("ODMR");
#endif /* ODMR_ENABLE */
    default: return("unknown?!?");
    }
}

char *visbuf(const char *buf)
/* visibilize a given string */
{
    static char vbuf[BUFSIZ];
    char *tp = vbuf;

    while (*buf)
    {
	if (*buf == '"')
	{
	    *tp++ = '\\'; *tp++ = '"';
	    buf++;
	}
	else if (*buf == '\\')
	{
	    *tp++ = '\\'; *tp++ = '\\';
	    buf++;
	}
	else if (isprint(*buf) || *buf == ' ')
	    *tp++ = *buf++;
	else if (*buf == '\n')
	{
	    *tp++ = '\\'; *tp++ = 'n';
	    buf++;
	}
	else if (*buf == '\r')
	{
	    *tp++ = '\\'; *tp++ = 'r';
	    buf++;
	}
	else if (*buf == '\b')
	{
	    *tp++ = '\\'; *tp++ = 'b';
	    buf++;
	}
	else if (*buf < ' ')
	{
	    *tp++ = '\\'; *tp++ = '^'; *tp++ = '@' + *buf;
	    buf++;
	}
	else
	{
	    (void) sprintf(tp, "\\0x%02x", *buf++);
	    tp += strlen(tp);
	}
    }
    *tp++ = '\0';
    return(vbuf);
}

/* env.c ends here */
