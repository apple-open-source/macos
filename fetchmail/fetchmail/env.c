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
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "fetchmail.h"
#include "getaddrinfo.h"

#include "i18n.h"
#if defined(HAVE_SETLOCALE) && defined(ENABLE_NLS) && defined(HAVE_STRFTIME)
#include <locale.h>
#endif

extern char *getenv(const char *);	/* needed on sysV68 R3V7.1. */

void envquery(int argc, char **argv)
/* set up basic stuff from the environment (including the rc file name) */
{
    struct passwd by_name, by_uid, *pwp;

    (void)argc;

    (void)argc;
    if (!(user = getenv("FETCHMAILUSER")))
    {
	if (!(user = getenv("LOGNAME")))
	{
	    user = getenv("USER");
	}
    }

    if (argv[0] == NULL)
    {
	    fprintf(stderr, "fetchmail: bad program name\n");
	    exit(PS_UNDEFINED);
    }

    if ((program_name = strrchr(argv[0], '/')) != NULL)
	++program_name;
    else
	program_name = argv[0];

    if (getenv("QMAILINJECT") && strcmp(getenv("QMAILINJECT"), ""))
    {
	fprintf(stderr,
		GT_("%s: The QMAILINJECT environment variable is set.\n"
		    "This is dangerous as it can make qmail-inject or qmail's sendmail wrapper\n"  
		    "tamper with your From: or Message-ID: headers.\n"
		    "Try \"env QMAILINJECT= %s YOUR ARGUMENTS HERE\"\n"
		    "%s: Abort.\n"), 
		program_name, program_name, program_name);
	exit(PS_UNDEFINED);
    }

    if (getenv("NULLMAILER_FLAGS") && strcmp(getenv("NULLMAILER_FLAGS"), ""))
    {
	fprintf(stderr,
		GT_("%s: The NULLMAILER_FLAGS environment variable is set.\n"
		    "This is dangerous as it can make nullmailer-inject or nullmailer's\n" 
		    "sendmail wrapper tamper with your From:, Message-ID: or Return-Path: headers.\n"
		    "Try \"env NULLMAILER_FLAGS= %s YOUR ARGUMENTS HERE\"\n"
		    "%s: Abort.\n"), 
		program_name, program_name, program_name);
	exit(PS_UNDEFINED);
    }

    if (!(pwp = getpwuid(getuid())))
    {
	fprintf(stderr,
		GT_("%s: You don't exist.  Go away.\n"),
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
    home = getenv("HOME_ETC");
    if (!home && !(home = getenv("HOME")))
	home = xstrdup(pwp->pw_dir);

    /* compute fetchmail's home directory */
    if (!(fmhome = getenv("FETCHMAILHOME")))
	fmhome = home;

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

char *host_fqdn(int required)
{
    char tmpbuf[HOSTLEN+1];
    char *result;

    if (gethostname(tmpbuf, sizeof(tmpbuf)))
    {
	fprintf(stderr, GT_("%s: can't determine your host!"),
		program_name);
	exit(PS_DNS);
    }

    /* if we got no . in the hostname, try to canonicalize it,
     * else assume it is a FQDN */
    if (strchr(tmpbuf, '.') == NULL)
    {
	/* if we got a basename without dots, as we often do in Linux,
	 * look up canonical name (make a FQDN of it) */
	struct addrinfo hints, *res;
	int e;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;

	e = fm_getaddrinfo(tmpbuf, NULL, &hints, &res);
	if (e) {
	    /* exit with error message */
	    fprintf(stderr,
		    GT_("gethostbyname failed for %s\n"), tmpbuf);
	    fprintf(stderr, "%s", gai_strerror(e));
	    fprintf(stderr, GT_("Cannot find my own host in hosts database to qualify it!\n"));
	    if (required)
		exit(PS_DNS);
	    else {
		fprintf(stderr, GT_("Trying to continue with unqualified hostname.\nDO NOT report broken Received: headers, HELO/EHLO lines or similar problems!\nDO repair your /etc/hosts, DNS, NIS or LDAP instead.\n"));
		return xstrdup(tmpbuf);
	    }
	}

	result = xstrdup(res->ai_canonname ? res->ai_canonname : tmpbuf);
	fm_freeaddrinfo(res);
    }
    else
	result = xstrdup(tmpbuf);

    return result;
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
    snprintf(offset_string, sizeof(offset_string),
	    "%c%02d%02d", sign, off / 60, off % 60);
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
     * weird multibyte i18n characters (such as kanji) from showing up
     * in your Received headers.
     */
#if defined(HAVE_SETLOCALE) && defined(ENABLE_NLS)
    setlocale (LC_TIME, "C");
#endif
    strftime(buf, sizeof(buf)-1, 
	     "%a, %d %b %Y %H:%M:%S XXXXX (%Z)", localtime(&now));
#if defined(HAVE_SETLOCALE) && defined(ENABLE_NLS)
    setlocale (LC_TIME, "");
#endif
    strncpy(strstr(buf, "XXXXX"), tzoffset(&now), 5);
#else
    /*
     * This is really just a portability fallback, as the
     * date format ctime(3) emits is not RFC822
     * conformant.
     */
    strlcpy(buf, ctime(&now), sizeof(buf));
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
    static char *vbuf;
    static size_t vbufs;
    char *tp;
    size_t needed;

    needed = strlen(buf) * 5 + 1; /* worst case: HEX, plus NUL byte */

    if (needed > vbufs) {
	vbufs = needed;
	vbuf = (char *)xrealloc(vbuf, vbufs);
    }

    tp = vbuf;

    while (*buf)
    {
	     if (*buf == '"')  { *tp++ = '\\'; *tp++ = '"'; buf++; }
	else if (*buf == '\\') { *tp++ = '\\'; *tp++ = '\\'; buf++; }
	else if (isprint((unsigned char)*buf) || *buf == ' ') *tp++ = *buf++;
	else if (*buf == '\a') { *tp++ = '\\'; *tp++ = 'a'; buf++; }
	else if (*buf == '\b') { *tp++ = '\\'; *tp++ = 'b'; buf++; }
	else if (*buf == '\f') { *tp++ = '\\'; *tp++ = 'f'; buf++; }
	else if (*buf == '\n') { *tp++ = '\\'; *tp++ = 'n'; buf++; }
	else if (*buf == '\r') { *tp++ = '\\'; *tp++ = 'r'; buf++; }
	else if (*buf == '\t') { *tp++ = '\\'; *tp++ = 't'; buf++; }
	else if (*buf == '\v') { *tp++ = '\\'; *tp++ = 'v'; buf++; }
	else
	{
	    const char hex[] = "0123456789abcdef";
	    *tp++ = '\\'; *tp++ = '0'; *tp++ = 'x';
	    *tp++ = hex[*buf >> 4];
	    *tp++ = hex[*buf & 0xf];
	    buf++;
	}
    }
    *tp++ = '\0';
    return(vbuf);
}
/* env.c ends here */
