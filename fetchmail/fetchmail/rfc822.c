/*
 * rfc822.c -- code for slicing and dicing RFC822 mail headers
 *
 * Copyright 1997 by Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 */

#include  <stdio.h>
#include  <ctype.h>
#include  <string.h>
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#endif

#include "config.h"
#include "fetchmail.h"
#include "i18n.h"

#define HEADER_END(p)	((p)[0] == '\n' && ((p)[1] != ' ' && (p)[1] != '\t'))

#ifdef TESTMAIN
static int verbose;
char *program_name = "rfc822";
#endif /* TESTMAIN */

unsigned char *reply_hack(buf, host)
/* hack message headers so replies will work properly */
unsigned char *buf;		/* header to be hacked */
const unsigned char *host;	/* server hostname */
{
    unsigned char *from, *cp, last_nws = '\0', *parens_from = NULL;
    int parendepth, state, has_bare_name_part, has_host_part;
#ifndef TESTMAIN
    int addresscount = 1;
#endif /* TESTMAIN */

    if (strncasecmp("From:", buf, 5)
	&& strncasecmp("To:", buf, 3)
	&& strncasecmp("Reply-To:", buf, 9)
	&& strncasecmp("Return-Path:", buf, 12)
	&& strncasecmp("Cc:", buf, 3)
	&& strncasecmp("Bcc:", buf, 4)
	&& strncasecmp("Resent-From:", buf, 12)
	&& strncasecmp("Resent-To:", buf, 10)
	&& strncasecmp("Resent-Cc:", buf, 10)
	&& strncasecmp("Resent-Bcc:", buf, 11)
	&& strncasecmp("Apparently-From:", buf, 16)
	&& strncasecmp("Apparently-To:", buf, 14)
	&& strncasecmp("Sender:", buf, 7)
	&& strncasecmp("Resent-Sender:", buf, 14)
       ) {
	return(buf);
    }

#ifndef TESTMAIN
    if (outlevel >= O_DEBUG)
	report_build(stdout, _("About to rewrite %s"), buf);

    /* make room to hack the address; buf must be malloced */
    for (cp = buf; *cp; cp++)
	if (*cp == ',' || isspace(*cp))
	    addresscount++;
    buf = (unsigned char *)xrealloc(buf, strlen(buf) + addresscount * strlen(host) + 1);
#endif /* TESTMAIN */

    /*
     * This is going to foo up on some ill-formed addresses.
     * Note that we don't rewrite the fake address <> in order to
     * avoid screwing up bounce suppression with a null Return-Path.
     */

    parendepth = state = 0;
    has_host_part = has_bare_name_part = FALSE;
    for (from = buf; *from; from++)
    {
#ifdef TESTMAIN
	if (verbose)
	{
	    printf("state %d: %s", state, buf);
	    printf("%*s^\n", from - buf + 10, " ");
	}
#endif /* TESTMAIN */
	if (state != 2)
	{
	    if (*from == '(')
		++parendepth;
	    else if (*from == ')')
		--parendepth;
	}

	if (!parendepth && !has_host_part)
	    switch (state)
	    {
	    case 0:	/* before header colon */
		if (*from == ':')
		    state = 1;
		break;

	    case 1:	/* we've seen the colon, we're looking for addresses */
		if (!isspace(*from))
		    last_nws = *from;
		if (*from == '<')
		    state = 3;
		else if (*from == '@' || *from == '!')
		    has_host_part = TRUE;
		else if (*from == '"')
		    state = 2;
		/*
		 * Not expanding on last non-WS == ';' deals with groupnames,
		 * an obscure misfeature described in sections
		 * 6.1, 6.2.6, and A.1.5 of the RFC822 standard.
		 */
		else if ((*from == ',' || HEADER_END(from))
			 && has_bare_name_part
			 && !has_host_part
			 && last_nws != ';')
		{
		    int hostlen;
		    unsigned char *p;

		    p = from;
		    if (parens_from)
			from = parens_from;
		    while (isspace(*from) || (*from == ','))
			--from;
		    from++;
		    hostlen = strlen(host);
		    for (cp = from + strlen(from); cp >= from; --cp)
			cp[hostlen+1] = *cp;
		    *from++ = '@';
		    memcpy(from, host, hostlen);
		    from = p + hostlen + 1;
		    has_host_part = TRUE;
		} 
		else if (from[1] == '('
			 && has_bare_name_part
			 && !has_host_part
			 && last_nws != ';' && last_nws != ')')
		{
		    parens_from = from;
		} 
		else if (!isspace(*from))
		    has_bare_name_part = TRUE;
		break;

	    case 2:	/* we're in a string */
		if (*from == '"')
		{
		    char	*bp;
		    int		bscount;

		    bscount = 0;
		    for (bp = from - 1; *bp == '\\'; bp--)
			bscount++;
		    if (!(bscount % 2))
			state = 1;
		}
		break;

	    case 3:	/* we're in a <>-enclosed address */
		if (*from == '@' || *from == '!')
		    has_host_part = TRUE;
		else if (*from == '>' && from[-1] != '<')
		{
		    state = 1;
		    if (!has_host_part)
		    {
			int hostlen;

			hostlen = strlen(host);
			for (cp = from + strlen(from); cp >= from; --cp)
			    cp[hostlen+1] = *cp;
			*from++ = '@';
			memcpy(from, host, hostlen);
			from += hostlen;
			has_host_part = TRUE;
		    }
		}
		break;
	    }

	/*
	 * If we passed a comma, reset everything.
	 */
	if (from[-1] == ',' && !parendepth) {
	  has_host_part = has_bare_name_part = FALSE;
	  parens_from = NULL;
	}
    }

#ifndef TESTMAIN
    if (outlevel >= O_DEBUG)
	report_complete(stdout, _("Rewritten version is %s\n"), buf);
#endif /* TESTMAIN */
    return(buf);
}

unsigned char *nxtaddr(hdr)
/* parse addresses in succession out of a specified RFC822 header */
const unsigned char *hdr;	/* header to be parsed, NUL to continue previous hdr */
{
    static unsigned char address[POPBUFSIZE+1];
    static int tp;
    static const unsigned char *hp;
    static int	state, oldstate;
#ifdef TESTMAIN
    static const unsigned char *orighdr;
#endif /* TESTMAIN */
    int parendepth = 0;

#define START_HDR	0	/* before header colon */
#define SKIP_JUNK	1	/* skip whitespace, \n, and junk */
#define BARE_ADDRESS	2	/* collecting address without delimiters */
#define INSIDE_DQUOTE	3	/* inside double quotes */
#define INSIDE_PARENS	4	/* inside parentheses */
#define INSIDE_BRACKETS	5	/* inside bracketed address */
#define ENDIT_ALL	6	/* after last address */

#define NEXTTP()	((tp < sizeof(address)-1) ? tp++ : tp)

    if (hdr)
    {
	hp = hdr;
	state = START_HDR;
#ifdef TESTMAIN
	orighdr = hdr;
#endif /* TESTMAIN */
	tp = 0;
    }

    for (; *hp; hp++)
    {
#ifdef TESTMAIN
	if (verbose)
	{
	    printf("state %d: %s", state, orighdr);
	    printf("%*s^\n", hp - orighdr + 10, " ");
	}
#endif /* TESTMAIN */

	if (state == ENDIT_ALL)		/* after last address */
	    return(NULL);
	else if (HEADER_END(hp))
	{
	    state = ENDIT_ALL;
	    if (tp)
	    {
		while (isspace(address[--tp]))
		    continue;
		address[++tp] = '\0';
		tp = 0;
		return (address);
	    }
	    return((unsigned char *)NULL);
	}
	else if (*hp == '\\')		/* handle RFC822 escaping */
	{
	    if (state != INSIDE_PARENS)
	    {
		address[NEXTTP()] = *hp++;	/* take the escape */
		address[NEXTTP()] = *hp;	/* take following unsigned char */
	    }
	}
	else switch (state)
	{
	case START_HDR:   /* before header colon */
	    if (*hp == ':')
		state = SKIP_JUNK;
	    break;

	case SKIP_JUNK:		/* looking for address start */
	    if (*hp == '"')	/* quoted string */
	    {
		oldstate = SKIP_JUNK;
	        state = INSIDE_DQUOTE;
		address[NEXTTP()] = *hp;
	    }
	    else if (*hp == '(')	/* address comment -- ignore */
	    {
		parendepth = 1;
		oldstate = SKIP_JUNK;
		state = INSIDE_PARENS;    
	    }
	    else if (*hp == '<')	/* begin <address> */
	    {
		state = INSIDE_BRACKETS;
		tp = 0;
	    }
	    else if (*hp != ',' && !isspace(*hp))
	    {
		--hp;
	        state = BARE_ADDRESS;
	    }
	    break;

	case BARE_ADDRESS:   	/* collecting address without delimiters */
	    if (*hp == ',')  	/* end of address */
	    {
		if (tp)
		{
		    address[NEXTTP()] = '\0';
		    state = SKIP_JUNK;
		    tp = 0;
		    return(address);
		}
	    }
	    else if (*hp == '(')  	/* beginning of comment */
	    {
		parendepth = 1;
		oldstate = BARE_ADDRESS;
		state = INSIDE_PARENS;    
	    }
	    else if (*hp == '<')  	/* beginning of real address */
	    {
		state = INSIDE_BRACKETS;
		tp = 0;
	    }
	    else if (!isspace(*hp)) 	/* just take it, ignoring whitespace */
		address[NEXTTP()] = *hp;
	    break;

	case INSIDE_DQUOTE:	/* we're in a quoted string, copy verbatim */
	    if (*hp != '"')
	        address[NEXTTP()] = *hp;
	    else
	    {
	        address[NEXTTP()] = *hp;
		state = oldstate;
	    }
	    break;

	case INSIDE_PARENS:	/* we're in a parenthesized comment, ignore */
	    if (*hp == '(')
		++parendepth;
	    else if (*hp == ')')
		--parendepth;
	    if (parendepth == 0)
		state = oldstate;
	    break;

	case INSIDE_BRACKETS:	/* possible <>-enclosed address */
	    if (*hp == '>')	/* end of address */
	    {
		address[NEXTTP()] = '\0';
		state = SKIP_JUNK;
		++hp;
		tp = 0;
		return(address);
	    }
	    else if (*hp == '<')	/* nested <> */
	        tp = 0;
	    else if (*hp == '"')	/* quoted address */
	    {
	        address[NEXTTP()] = *hp;
		oldstate = INSIDE_BRACKETS;
		state = INSIDE_DQUOTE;
	    }
	    else			/* just copy address */
		address[NEXTTP()] = *hp;
	    break;
	}
    }

    return(NULL);
}

#ifdef TESTMAIN
static void parsebuf(unsigned char *longbuf, int reply)
{
    unsigned char	*cp;

    if (reply)
    {
	reply_hack(longbuf, "HOSTNAME.NET");
	printf("Rewritten buffer: %s", longbuf);
    }
    else
	if ((cp = nxtaddr(longbuf)) != (unsigned char *)NULL)
	    do {
		printf("\t-> \"%s\"\n", cp);
	    } while
		((cp = nxtaddr((unsigned char *)NULL)) != (unsigned char *)NULL);
}



main(int argc, char *argv[])
{
    unsigned char	buf[MSGBUFSIZE], longbuf[BUFSIZ];
    int			ch, reply;
    
    verbose = reply = FALSE;
    while ((ch = getopt(argc, argv, "rv")) != EOF)
	switch(ch)
	{
	case 'r':
	    reply = TRUE;
	    break;

	case 'v':
	    verbose = TRUE;
	    break;
	}

    while (fgets(buf, sizeof(buf)-1, stdin))
    {
	if (buf[0] == ' ' || buf[0] == '\t')
	    strcat(longbuf, buf);
	else if (!strncasecmp("From: ", buf, 6)
		    || !strncasecmp("To: ", buf, 4)
		    || !strncasecmp("Reply-", buf, 6)
		    || !strncasecmp("Cc: ", buf, 4)
		    || !strncasecmp("Bcc: ", buf, 5))
	    strcpy(longbuf, buf);	
	else if (longbuf[0])
	{
	    if (verbose)
		fputs(longbuf, stdout);
	    parsebuf(longbuf, reply);
	    longbuf[0] = '\0';
	}
    }
    if (longbuf[0])
    {
	if (verbose)
	    fputs(longbuf, stdout);
	parsebuf(longbuf, reply);
    }
}
#endif /* TESTMAIN */

/* rfc822.c end */
