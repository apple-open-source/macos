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

#define HEADER_END(p)	((p)[0] == '\n' && ((p)[1] != ' ' && (p)[1] != '\t'))

#ifdef TESTMAIN
static int verbose;
char *program_name = "rfc822";
#endif /* TESTMAIN */

char *reply_hack(buf, host)
/* hack message headers so replies will work properly */
char *buf;		/* header to be hacked */
const char *host;	/* server hostname */
{
    char *from, *cp, last_nws = '\0', *parens_from = NULL;
    int parendepth, state, has_bare_name_part, has_host_part;
#ifndef TESTMAIN
    int addresscount = 1;
#endif /* TESTMAIN */

    if (strncasecmp("From: ", buf, 6)
	&& strncasecmp("To: ", buf, 4)
	&& strncasecmp("Reply-To: ", buf, 10)
	&& strncasecmp("Return-Path: ", buf, 13)
	&& strncasecmp("Cc: ", buf, 4)
	&& strncasecmp("Bcc: ", buf, 5)
	&& strncasecmp("Resent-From: ", buf, 13)
	&& strncasecmp("Resent-To: ", buf, 11)
	&& strncasecmp("Resent-Cc: ", buf, 11)
	&& strncasecmp("Resent-Bcc: ", buf, 12)
	&& strncasecmp("Apparently-From:", buf, 16)
	&& strncasecmp("Apparently-To:", buf, 14)
	&& strncasecmp("Sender:", buf, 7)
	&& strncasecmp("Resent-Sender:", buf, 14)
       ) {
	return(buf);
    }

#ifndef TESTMAIN
    if (outlevel >= O_DEBUG)
	report_build(stdout, "About to rewrite %s", buf);

    /* make room to hack the address; buf must be malloced */
    for (cp = buf; *cp; cp++)
	if (*cp == ',' || isspace(*cp))
	    addresscount++;
    buf = (char *)xrealloc(buf, strlen(buf) + addresscount * strlen(host) + 1);
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
	    if (*from == '(')
		++parendepth;
	    else if (*from == ')')
		--parendepth;

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
		else if (*from == '@')
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
		    char *p;

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
		    state = 1;
		break;

	    case 3:	/* we're in a <>-enclosed address */
		if (*from == '@')
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
	report_complete(stdout, "Rewritten version is %s\n", buf);
#endif /* TESTMAIN */
    return(buf);
}

char *nxtaddr(hdr)
/* parse addresses in succession out of a specified RFC822 header */
const char *hdr;	/* header to be parsed, NUL to continue previous hdr */
{
    static char *tp, address[POPBUFSIZE+1];
    static const char *hp;
    static int	state, oldstate;
#ifdef TESTMAIN
    static const char *orighdr;
#endif /* TESTMAIN */
    int parendepth = 0;

#define START_HDR	0	/* before header colon */
#define SKIP_JUNK	1	/* skip whitespace, \n, and junk */
#define BARE_ADDRESS	2	/* collecting address without delimiters */
#define INSIDE_DQUOTE	3	/* inside double quotes */
#define INSIDE_PARENS	4	/* inside parentheses */
#define INSIDE_BRACKETS	5	/* inside bracketed address */
#define ENDIT_ALL	6	/* after last address */

    if (hdr)
    {
	hp = hdr;
	state = START_HDR;
#ifdef TESTMAIN
	orighdr = hdr;
#endif /* TESTMAIN */
	tp = address;
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
	    if (tp > address)
	    {
		while (isspace(*--tp))
		    continue;
		*++tp = '\0';
	    }
	    return(tp > address ? (tp = address) : (char *)NULL);
	}
	else if (*hp == '\\')		/* handle RFC822 escaping */
	{
	    if (state != INSIDE_PARENS)
	    {
		*tp++ = *hp++;			/* take the escape */
		*tp++ = *hp;			/* take following char */
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
		*tp++ = *hp;
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
		tp = address;
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
		if (tp > address)
		{
		    *tp++ = '\0';
		    state = SKIP_JUNK;
		    return(tp = address);
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
		tp = address;
	    }
	    else if (!isspace(*hp)) 	/* just take it, ignoring whitespace */
		*tp++ = *hp;
	    break;

	case INSIDE_DQUOTE:	/* we're in a quoted string, copy verbatim */
	    if (*hp != '"')
	        *tp++ = *hp;
	    else
	    {
	        *tp++ = *hp;
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
		*tp++ = '\0';
		state = SKIP_JUNK;
		++hp;
		return(tp = address);
	    }
	    else if (*hp == '<')	/* nested <> */
	        tp = address;
	    else if (*hp == '"')	/* quoted address */
	    {
	        *tp++ = *hp;
		oldstate = INSIDE_BRACKETS;
		state = INSIDE_DQUOTE;
	    }
	    else			/* just copy address */
		*tp++ = *hp;
	    break;
	}
    }

    return(NULL);
}

#ifdef TESTMAIN
static void parsebuf(char *longbuf, int reply)
{
    char	*cp;

    if (reply)
    {
	reply_hack(longbuf, "HOSTNAME.NET");
	printf("Rewritten buffer: %s", longbuf);
    }
    else
	if ((cp = nxtaddr(longbuf)) != (char *)NULL)
	    do {
		printf("\t-> \"%s\"\n", cp);
	    } while
		((cp = nxtaddr((char *)NULL)) != (char *)NULL);
}



main(int argc, char *argv[])
{
    char	buf[MSGBUFSIZE], longbuf[BUFSIZ];
    int		ch, reply;
    
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
