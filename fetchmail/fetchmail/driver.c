/*
 * driver.c -- generic driver for mail fetch method protocols
 *
 * Copyright 1997 by Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 */

#include  "config.h"
#include  <stdio.h>
#include  <setjmp.h>
#include  <errno.h>
#include  <ctype.h>
#include  <string.h>
#ifdef HAVE_MEMORY_H
#include  <memory.h>
#endif /* HAVE_MEMORY_H */
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(HAVE_STDARG_H)
#include  <stdarg.h>
#else
#include  <varargs.h>
#endif
#if defined(HAVE_SYS_ITIMER_H)
#include <sys/itimer.h>
#endif
#include  <sys/time.h>
#include  <signal.h>

#ifdef HAVE_RES_SEARCH
#include <netdb.h>
#include "mx.h"
#endif /* HAVE_RES_SEARCH */

#ifdef KERBEROS_V4
#ifdef KERBEROS_V5
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#else
#if defined (__bsdi__)
#include <des.h> /* order of includes matters */
#include <krb.h>
#define krb_get_err_text(e) (krb_err_txt[e])
#else
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__linux__)
#define krb_get_err_text(e) (krb_err_txt[e])
#include <krb.h>
#include <des.h>
#else
#include <krb.h>
#include <des.h>
#endif /* ! defined (__FreeBSD__) */
#endif /* ! defined (__bsdi__) */
#endif /* KERBEROS_V5 */
#include <netinet/in.h>
#include <netdb.h>
#endif /* KERBEROS_V4 */
#ifdef KERBEROS_V5
#include <krb5.h>
#include <com_err.h>
#endif /* KERBEROS_V5 */
#include "i18n.h"

#include "socket.h"
#include "fetchmail.h"
#include "tunable.h"

/* throw types for runtime errors */
#define THROW_TIMEOUT	1		/* server timed out */
#define THROW_SIGPIPE	2		/* SIGPIPE on stream socket */

#ifndef strstr		/* glibc-2.1 declares this as a macro */
extern char *strstr();	/* needed on sysV68 R3V7.1. */
#endif /* strstr */

int batchcount;		/* count of messages sent in current batch */
flag peek_capable;	/* can we peek for better error recovery? */
int pass;		/* how many times have we re-polled? */
int phase;		/* where are we, for error-logging purposes? */

static const struct method *protocol;
static jmp_buf	restart;

char tag[TAGLEN];
static int tagnum;
#define GENSYM	(sprintf(tag, "A%04d", ++tagnum % TAGMOD), tag)

static char shroud[PASSWORDLEN];	/* string to shroud in debug output */
static int mytimeout;			/* value of nonreponse timeout */
static int timeoutcount;		/* count consecutive timeouts */
static int msglen;			/* actual message length */

void set_timeout(int timeleft)
/* reset the nonresponse-timeout */
{
#ifndef __EMX__
    struct itimerval ntimeout;

    if (timeleft == 0)
	timeoutcount = 0;

    ntimeout.it_interval.tv_sec = ntimeout.it_interval.tv_usec = 0;
    ntimeout.it_value.tv_sec  = timeleft;
    ntimeout.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &ntimeout, (struct itimerval *)NULL);
#endif
}

static void timeout_handler (int signal)
/* handle SIGALRM signal indicating a server timeout */
{
    timeoutcount++;
    longjmp(restart, THROW_TIMEOUT);
}

static void sigpipe_handler (int signal)
/* handle SIGPIPE signal indicating a broken stream socket */
{
    longjmp(restart, THROW_SIGPIPE);
}

static int accept_count, reject_count;

static void map_name(const char *name, struct query *ctl, struct idlist **xmit_names)
/* add given name to xmit_names if it matches declared localnames */
/*   name:	 name to map */
/*   ctl:	 list of permissible aliases */
/*   xmit_names: list of recipient names parsed out */
{
    const char	*lname;
    int off = 0;
    
    lname = idpair_find(&ctl->localnames, name+off);
    if (!lname && ctl->wildcard)
	lname = name+off;

    if (lname != (char *)NULL)
    {
	if (outlevel >= O_DEBUG)
	    report(stdout, _("mapped %s to local %s\n"), name, lname);
	save_str(xmit_names, lname, XMIT_ACCEPT);
	accept_count++;
    }
}

static void find_server_names(const char *hdr,
			      struct query *ctl,
			      struct idlist **xmit_names)
/* parse names out of a RFC822 header into an ID list */
/*   hdr:		RFC822 header in question */
/*   ctl:		list of permissible aliases */
/*   xmit_names:	list of recipient names parsed out */
{
    if (hdr == (char *)NULL)
	return;
    else
    {
	char	*cp;

	for (cp = nxtaddr(hdr);
	     cp != NULL;
	     cp = nxtaddr(NULL))
	{
	    char	*atsign;

	    /*
	     * If the name of the user begins with a qmail virtual
	     * domain prefix, ignore the prefix.  Doing this here
	     * means qvirtual will work either with ordinary name
	     * mapping or with a localdomains option.
	     */
	    if (ctl->server.qvirtual)
	    {
		int sl = strlen(ctl->server.qvirtual);
 
		if (!strncasecmp(cp, ctl->server.qvirtual, sl))
		    cp += sl;
	    }

	    if ((atsign = strchr(cp, '@'))) {
		struct idlist	*idp;

		/*
		 * Does a trailing segment of the hostname match something
		 * on the localdomains list?  If so, save the whole name
		 * and keep going.
		 */
		for (idp = ctl->server.localdomains; idp; idp = idp->next) {
		    char	*rhs;

		    rhs = atsign + (strlen(atsign) - strlen(idp->id));
		    if (rhs > atsign &&
			(rhs[-1] == '.' || rhs[-1] == '@') &&
			strcasecmp(rhs, idp->id) == 0)
		    {
			if (outlevel >= O_DEBUG)
			    report(stdout, _("passed through %s matching %s\n"), 
				  cp, idp->id);
			save_str(xmit_names, cp, XMIT_ACCEPT);
			accept_count++;
			goto nomap;
		    }
		}

		/* if we matched a local domain, idp != NULL */
		if (!idp)
		{
		    /*
		     * Check to see if the right-hand part is an alias
		     * or MX equivalent of the mailserver.  If it's
		     * not, skip this name.  If it is, we'll keep
		     * going and try to find a mapping to a client name.
		     */
		    if (!is_host_alias(atsign+1, ctl))
		    {
			save_str(xmit_names, cp, XMIT_REJECT);
			reject_count++;
			continue;
		    }
		}
		atsign[0] = '\0';
		map_name(cp, ctl, xmit_names);
	    nomap:;
	    }
	}
    }
}

static char *parse_received(struct query *ctl, char *bufp)
/* try to extract real address from the Received line */
/* If a valid Received: line is found, we return the full address in
 * a buffer which can be parsed from nxtaddr().  This is to ansure that
 * the local domain part of the address can be passed along in 
 * find_server_names() if it contains one.
 * Note: We should return a dummy header containing the address 
 * which makes nxtaddr() behave correctly. 
 */
{
    char *base, *ok = (char *)NULL;
    static char rbuf[HOSTLEN + USERNAMELEN + 4]; 

    /*
     * Try to extract the real envelope addressee.  We look here
     * specifically for the mailserver's Received line.
     * Note: this will only work for sendmail, or an MTA that
     * shares sendmail's convention for embedding the envelope
     * address in the Received line.  Sendmail itself only
     * does this when the mail has a single recipient.
     */
    if (outlevel >= O_DEBUG)
	report(stdout, _("analyzing Received line:\n%s"), bufp);

    /* search for whitepace-surrounded "by" followed by xxxx.yyyy */
    for (base = bufp;  ; base = ok + 2)
    {
	if (!(ok = strstr(base, "by")))
	    break;
	else if (!isspace(ok[-1]) || !isspace(ok[2]))
	    continue;
	else
	{
	    char	*sp, *tp;

	    /* extract space-delimited token after "by" */
	    for (sp = ok + 2; isspace(*sp); sp++)
		continue;
	    tp = rbuf;
	    for (; !isspace(*sp); sp++)
		*tp++ = *sp;
	    *tp = '\0';

	    /* look for embedded periods */
	    if (strchr(rbuf, '.'))
		break;
	    else
		ok = sp - 1;	/* arrange to skip this token */
	}
    }
    if (ok)
    {
	/*
	 * If it's a DNS name of the mail server, look for the
	 * recipient name after a following "for".  Otherwise
	 * punt.
	 */
	if (is_host_alias(rbuf, ctl))
	{
	    if (outlevel >= O_DEBUG)
		report(stdout, 
		      _("line accepted, %s is an alias of the mailserver\n"), rbuf);
	}
	else
	{
	    if (outlevel >= O_DEBUG)
		report(stdout, 
		      _("line rejected, %s is not an alias of the mailserver\n"), 
		      rbuf);
	    return(NULL);
	}

	/* search for whitepace-surrounded "for" followed by xxxx@yyyy */
	for (base = ok + 4 + strlen(rbuf);  ; base = ok + 2)
	{
	    if (!(ok = strstr(base, "for")))
		break;
	    else if (!isspace(ok[-1]) || !isspace(ok[3]))
		continue;
	    else
	    {
		char	*sp, *tp;

		/* extract space-delimited token after "for" */
		for (sp = ok + 3; isspace(*sp); sp++)
		    continue;
		tp = rbuf;
		for (; !isspace(*sp); sp++)
		    *tp++ = *sp;
		*tp = '\0';

		if (strchr(rbuf, '@'))
		    break;
		else
		    ok = sp - 1;	/* arrange to skip this token */
	    }
	}
	if (ok)
	{
	    flag	want_gt = FALSE;
	    char	*sp, *tp;

	    /* char after "for" could be space or a continuation newline */
	    for (sp = ok + 4; isspace(*sp); sp++)
		continue;
	    tp = rbuf;
	    *tp++ = ':';	/* Here is the hack.  This is to be friends */
	    *tp++ = ' ';	/* with nxtaddr()... */
	    if (*sp == '<')
	    {
		want_gt = TRUE;
		sp++;
	    }
	    while (*sp == '@')		/* skip routes */
		while (*sp && *sp++ != ':')
		    continue;
            while (*sp
                   && (want_gt ? (*sp != '>') : !isspace(*sp))
                   && *sp != ';')
		if (!isspace(*sp))
		    *tp++ = *sp++;
		else
		{
		    /* uh oh -- whitespace here can't be right! */
		    ok = (char *)NULL;
		    break;
		}
	    *tp++ = '\n';
	    *tp = '\0';
	    if (strlen(rbuf) <= 3)	/* apparently nothing has been found */
		ok = NULL;
	} else
	    ok = (char *)NULL;
    }

    if (!ok)
    {
	if (outlevel >= O_DEBUG)
	    report(stdout, _("no Received address found\n"));
	return(NULL);
    }
    else
    {
	if (outlevel >= O_DEBUG) {
	    char *lf = rbuf + strlen(rbuf)-1;
	    *lf = '\0';
	    if (outlevel >= O_DEBUG)
		report(stdout, _("found Received address `%s'\n"), rbuf+2);
	    *lf = '\n';
	}
	return(rbuf);
    }
}

/* shared by readheaders and readbody */
static int sizeticker;
static struct msgblk msgblk;

#define EMPTYLINE(s)	((s)[0] == '\r' && (s)[1] == '\n' && (s)[2] == '\0')

static int readheaders(int sock,
		       long fetchlen,
		       long reallen,
		       struct query *ctl,
		       int num)
/* read message headers and ship to SMTP or MDA */
/*   sock:		to which the server is connected */
/*   fetchlen:		length of message according to fetch response */
/*   reallen:		length of message according to getsizes */
/*   ctl:		query control record */
/*   num:		index of message */
{
    struct addrblk
    {
	int		offset;
	struct addrblk	*next;
    };
    struct addrblk	*to_addrchain = NULL;
    struct addrblk	**to_chainptr = &to_addrchain;
    struct addrblk	*resent_to_addrchain = NULL;
    struct addrblk	**resent_to_chainptr = &resent_to_addrchain;

    char		buf[MSGBUFSIZE+1];
    int			from_offs, reply_to_offs, resent_from_offs;
    int			app_from_offs, sender_offs, resent_sender_offs;
    int			env_offs;
    char		*received_for, *rcv, *cp;
    int 		n, linelen, oldlen, ch, remaining, skipcount;
    struct idlist 	*idp;
    flag		no_local_matches = FALSE;
    flag		headers_ok, has_nuls;
    int			olderrs, good_addresses, bad_addresses;

    sizeticker = 0;
    has_nuls = headers_ok = FALSE;
    msgblk.return_path[0] = '\0';
    olderrs = ctl->errcount;

    /* read message headers */
    msgblk.reallen = reallen;
    msgblk.headers = received_for = NULL;
    from_offs = reply_to_offs = resent_from_offs = app_from_offs = 
	sender_offs = resent_sender_offs = env_offs = -1;
    oldlen = 0;
    msglen = 0;
    skipcount = 0;
    ctl->mimemsg = 0;

    for (remaining = fetchlen; remaining > 0 || protocol->delimited; remaining -= linelen)
    {
	char *line;

	line = xmalloc(sizeof(buf));
	linelen = 0;
	line[0] = '\0';
	do {
	    set_timeout(mytimeout);
	    if ((n = SockRead(sock, buf, sizeof(buf)-1)) == -1) {
		set_timeout(0);
		free(line);
		free(msgblk.headers);
		return(PS_SOCKET);
	    }
	    set_timeout(0);
	    linelen += n;
	    msglen += n;

	    /* lines may not be properly CRLF terminated; fix this for qmail */
	    if (ctl->forcecr)
	    {
		cp = buf + strlen(buf) - 1;
		if (*cp == '\n' && (cp == buf || cp[-1] != '\r'))
		{
		    *cp++ = '\r';
		    *cp++ = '\n';
		    *cp++ = '\0';
		}
	    }

	    /*
	     * Decode MIME encoded headers. We MUST do this before
	     * looking at the Content-Type / Content-Transfer-Encoding
	     * headers (RFC 2046).
	     */
	    if (ctl->mimedecode)
		UnMimeHeader(buf);

	    line = (char *) realloc(line, strlen(line) + strlen(buf) +1);

	    strcat(line, buf);

	    /* check for end of headers */
	    if (EMPTYLINE(line))
	    {
		headers_ok = TRUE;
		has_nuls = (linelen != strlen(line));
		free(line);
		goto process_headers;
	    }

	    /*
	     * Check for end of message immediately.  If one of your folders
	     * has been mangled, the delimiter may occur directly after the
	     * header.
	     */
	    if (protocol->delimited && line[0] == '.' && EMPTYLINE(line+1))
	    {
		free(line);
		has_nuls = (linelen != strlen(line));
		goto process_headers;
	    }

	    /* check for RFC822 continuations */
	    set_timeout(mytimeout);
	    ch = SockPeek(sock);
	    set_timeout(0);
	} while
	    (ch == ' ' || ch == '\t');	/* continuation to next line? */

	/* write the message size dots */
	if ((outlevel > O_SILENT && outlevel < O_VERBOSE) && linelen > 0)
	{
	    sizeticker += linelen;
	    while (sizeticker >= SIZETICKER)
	    {
		if (!run.use_syslog)
		{
		    fputc('.', stdout);
		    fflush(stdout);
		}
		sizeticker -= SIZETICKER;
	    }
	}

	/* we see an ordinary (non-header, non-message-delimiter line */
	has_nuls = (linelen != strlen(line));

	/*
	 * When mail delivered to a multidrop mailbox on the server is
	 * addressed to multiple people, there will be one copy left
	 * in the box for each recipient.  Thus, if the mail is addressed
	 * to N people, each recipient would get N copies.
	 *
	 * Foil this by suppressing all but one copy of a message with
	 * a given Message-ID.  Note: This implementation only catches
	 * runs of successive identical messages, but that should be
	 * good enough.
	 */
	if (MULTIDROP(ctl) && !strncasecmp(line, "Message-ID:", 11))
	{
	    if (ctl->lastid && !strcasecmp(ctl->lastid, line))
		return(PS_REFUSED);
	    else
	    {
		if (ctl->lastid)
		    free(ctl->lastid);
		ctl->lastid = strdup(line);
	    }
	}

	/*
	 * The University of Washington IMAP server (the reference
	 * implementation of IMAP4 written by Mark Crispin) relies
	 * on being able to keep base-UID information in a special
	 * message at the head of the mailbox.  This message should
	 * neither be deleted nor forwarded.
	 */
#ifdef POP2_ENABLE
	/*
	 * We disable this check under POP2 because there's no way to
	 * prevent deletion of the message.  So at least we ought to 
	 * forward it to the user so he or she will have some clue
	 * that things have gone awry.
	 */
	if (protocol->port != 109)
#endif /* POP2_ENABLE */
	    if (num == 1 && !strncasecmp(line, "X-IMAP:", 7)) {
		free(line);
		free(msgblk.headers);
		return(PS_RETAINED);
	    }

	/*
	 * This code prevents fetchmail from becoming an accessory after
	 * the fact to upstream sendmails with the `E' option on.  It also
	 * copes with certain brain-dead POP servers (like NT's) that pass
	 * through Unix from_ lines.
	 *
	 * Either of these bugs can result in a non-RFC822 line at the
	 * beginning of the headers.  If fetchmail just passes it
	 * through, the client listener may think the message has *no*
	 * headers (since the first) line it sees doesn't look
	 * RFC822-conformant) and fake up a set.
	 *
	 * What the user would see in this case is bogus (synthesized)
	 * headers, followed by a blank line, followed by the >From, 
	 * followed by the real headers, followed by a blank line,
	 * followed by text.
	 *
	 * We forestall this lossage by tossing anything that looks
	 * like an escaped or passed-through From_ line in headers.
	 * These aren't RFC822 so our conscience is clear...
	 */
	if (!strncasecmp(line, ">From ", 6) || !strncasecmp(line, "From ", 5))
	{
	    free(line);
	    continue;
	}

	/*
	 * If we see a Status line, it may have been inserted by an MUA
	 * on the mail host, or it may have been inserted by the server
	 * program after the headers in the transaction stream.  This
	 * can actually hose some new-mail notifiers such as xbuffy,
	 * which assumes any Status line came from a *local* MDA and
	 * therefore indicates that the message has been seen.
	 *
	 * Some buggy POP servers (including at least the 3.3(20)
	 * version of the one distributed with IMAP) insert empty
	 * Status lines in the transaction stream; we'll chuck those
	 * unconditionally.  Nonempty ones get chucked if the user
	 * turns on the dropstatus flag.
	 */
	{
	    char	*cp;

	    if (!strncasecmp(line, "Status:", 7))
		cp = line + 7;
	    else if (!strncasecmp(line, "X-Mozilla-Status:", 17))
		cp = line + 17;
	    else
		cp = NULL;
	    if (cp) {
		while (*cp && isspace(*cp)) cp++;
		if (!*cp || ctl->dropstatus)
		{
		    free(line);
		    continue;
		}
	    }
	}

	if (ctl->rewrite)
	    line = reply_hack(line, ctl->server.truename);

	/*
	 * OK, this is messy.  If we're forwarding by SMTP, it's the
	 * SMTP-receiver's job (according to RFC821, page 22, section
	 * 4.1.1) to generate a Return-Path line on final delivery.
	 * The trouble is, we've already got one because the
	 * mailserver's SMTP thought *it* was responsible for final
	 * delivery.
	 *
	 * Stash away the contents of Return-Path (as modified by reply_hack)
	 * for use in generating MAIL FROM later on, then prevent the header
	 * from being saved with the others.  In effect, we strip it off here.
	 *
	 * If the SMTP server conforms to the standards, and fetchmail gets the
	 * envelope sender from the Return-Path, the new Return-Path should be
	 * exactly the same as the original one.
	 *
	 * We do *not* want to ignore empty Return-Path headers.  These should
	 * be passed through as a way of indicating that a message should
	 * not trigger bounces if delivery fails.  What we *do* need to do is
	 * make sure we never try to rewrite such a blank Return-Path.  We
	 * handle this with a check for <> in the rewrite logic above.
	 */
	if (!strncasecmp("Return-Path:", line, 12) && (cp = nxtaddr(line)))
	{
	    strcpy(msgblk.return_path, cp);
	    if (!ctl->mda) {
		free(line);
		continue;
	    }
	}

	if (!msgblk.headers)
	{
	    oldlen = strlen(line);
	    msgblk.headers = xmalloc(oldlen + 1);
	    (void) strcpy(msgblk.headers, line);
	    free(line);
	    line = msgblk.headers;
	}
	else
	{
	    int	newlen;

	    newlen = oldlen + strlen(line);
	    msgblk.headers = (char *) realloc(msgblk.headers, newlen + 1);
	    if (msgblk.headers == NULL) {
		free(line);
		return(PS_IOERR);
	    }
	    strcpy(msgblk.headers + oldlen, line);
	    free(line);
	    line = msgblk.headers + oldlen;
	    oldlen = newlen;
	}

	if (!strncasecmp("From:", line, 5))
	    from_offs = (line - msgblk.headers);
	else if (!strncasecmp("Reply-To:", line, 9))
	    reply_to_offs = (line - msgblk.headers);
	else if (!strncasecmp("Resent-From:", line, 12))
	    resent_from_offs = (line - msgblk.headers);
	else if (!strncasecmp("Apparently-From:", line, 16))
	    app_from_offs = (line - msgblk.headers);
	else if (!strncasecmp("Sender:", line, 7))
	    sender_offs = (line - msgblk.headers);
	else if (!strncasecmp("Resent-Sender:", line, 14))
	    resent_sender_offs = (line - msgblk.headers);

 	else if (!strncasecmp("Message-Id:", buf, 11))
	{
	    if (ctl->server.uidl)
 	    {
	        char id[IDLEN+1];

		buf[IDLEN+12] = 0;		/* prevent stack overflow */
 		sscanf(buf+12, "%s", id);
 	        if (!str_find( &ctl->newsaved, num))
		{
 		    struct idlist *new = save_str(&ctl->newsaved,id,UID_SEEN);
		    new->val.status.num = num;
		}
 	    }
 	}

	else if (!MULTIDROP(ctl))
	    continue;

	else if (!strncasecmp("To:", line, 3)
			|| !strncasecmp("Cc:", line, 3)
			|| !strncasecmp("Bcc:", line, 4)
			|| !strncasecmp("Apparently-To:", line, 14))
	{
	    *to_chainptr = xmalloc(sizeof(struct addrblk));
	    (*to_chainptr)->offset = (line - msgblk.headers);
	    to_chainptr = &(*to_chainptr)->next; 
	    *to_chainptr = NULL;
	}

	else if (!strncasecmp("Resent-To:", line, 10)
			|| !strncasecmp("Resent-Cc:", line, 10)
			|| !strncasecmp("Resent-Bcc:", line, 11))
	{
	    *resent_to_chainptr = xmalloc(sizeof(struct addrblk));
	    (*resent_to_chainptr)->offset = (line - msgblk.headers);
	    resent_to_chainptr = &(*resent_to_chainptr)->next; 
	    *resent_to_chainptr = NULL;
	}

	else if (ctl->server.envelope != STRING_DISABLED)
	{
	    if (ctl->server.envelope 
			&& strcasecmp(ctl->server.envelope, "Received"))
	    {
		if (env_offs == -1 && !strncasecmp(ctl->server.envelope,
						line,
						strlen(ctl->server.envelope)))
		{				
		    if (skipcount++ != ctl->server.envskip)
			continue;
		    env_offs = (line - msgblk.headers);
		}    
	    }
	    else if (!received_for && !strncasecmp("Received:", line, 9))
	    {
		if (skipcount++ != ctl->server.envskip)
		    continue;
		received_for = parse_received(ctl, line);
	    }
	}
    }

 process_headers:
    /*
     * We want to detect this early in case there are so few headers that the
     * dispatch logic barfs.
     */
    if (!headers_ok)
    {
	if (outlevel > O_SILENT)
	    report(stdout,
		   _("message delimiter found while scanning headers\n"));
    }

    /*
     * Hack time.  If the first line of the message was blank, with no headers
     * (this happens occasionally due to bad gatewaying software) cons up
     * a set of fake headers.  
     *
     * If you modify the fake header template below, be sure you don't
     * make either From or To address @-less, otherwise the reply_hack
     * logic will do bad things.
     */
    if (msgblk.headers == (char *)NULL)
    {
#ifdef HAVE_SNPRINTF
	snprintf(buf, sizeof(buf),
#else
	sprintf(buf, 
#endif /* HAVE_SNPRINTF */
	"From: FETCHMAIL-DAEMON\r\nTo: %s@%s\r\nSubject: Headerless mail from %s's mailbox on %s\r\n",
		user, fetchmailhost, ctl->remotename, ctl->server.truename);
	msgblk.headers = xstrdup(buf);
    }

    /*
     * We can now process message headers before reading the text.
     * In fact we have to, as this will tell us where to forward to.
     */

    /* Check for MIME headers indicating possible 8-bit data */
    ctl->mimemsg = MimeBodyType(msgblk.headers, ctl->mimedecode);

#ifdef SDPS_ENABLE
    if (ctl->server.sdps && sdps_envfrom)
    {
	/* We have the real envelope return-path, stored out of band by
	 * SDPS - that's more accurate than any header is going to be.
	 */
	strcpy(msgblk.return_path, sdps_envfrom);
	free(sdps_envfrom);
    } else
#endif /* SDPS_ENABLE */
    /*
     * If there is a Return-Path address on the message, this was
     * almost certainly the MAIL FROM address given the originating
     * sendmail.  This is the best thing to use for logging the
     * message origin (it sets up the right behavior for bounces and
     * mailing lists).  Otherwise, fall down to the next available 
     * envelope address (which is the most probable real sender).
     * *** The order is important! ***
     * This is especially useful when receiving mailing list
     * messages in multidrop mode.  if a local address doesn't
     * exist, the bounce message won't be returned blindly to the 
     * author or to the list itself but rather to the list manager
     * (ex: specified by "Sender:") which is much less annoying.  This 
     * is true for most mailing list packages.
     */
    if( !msgblk.return_path[0] ){
	char *ap = NULL;
	if (resent_sender_offs >= 0 && (ap = nxtaddr(msgblk.headers + resent_sender_offs)));
	else if (sender_offs >= 0 && (ap = nxtaddr(msgblk.headers + sender_offs)));
	else if (resent_from_offs >= 0 && (ap = nxtaddr(msgblk.headers + resent_from_offs)));
	else if (from_offs >= 0 && (ap = nxtaddr(msgblk.headers + from_offs)));
	else if (reply_to_offs >= 0 && (ap = nxtaddr(msgblk.headers + reply_to_offs)));
	else if (app_from_offs >= 0 && (ap = nxtaddr(msgblk.headers + app_from_offs)));
	if (ap) strcpy( msgblk.return_path, ap );
    }

    /* cons up a list of local recipients */
    msgblk.recipients = (struct idlist *)NULL;
    accept_count = reject_count = 0;
    /* is this a multidrop box? */
    if (MULTIDROP(ctl))
    {
#ifdef SDPS_ENABLE
	if (ctl->server.sdps && sdps_envto)
	{
	    /* We have the real envelope recipient, stored out of band by
	     * SDPS - that's more accurate than any header is going to be.
	     */
	    find_server_names(sdps_envto, ctl, &msgblk.recipients);
	    free(sdps_envto);
	} else
#endif /* SDPS_ENABLE */ 
	if (env_offs > -1)	    /* We have the actual envelope addressee */
	    find_server_names(msgblk.headers + env_offs, ctl, &msgblk.recipients);
	else if (received_for)
	    /*
	     * We have the Received for addressee.  
	     * It has to be a mailserver address, or we
	     * wouldn't have got here.
	     * We use find_server_names() to let local 
	     * hostnames go through.
	     */
	    find_server_names(received_for, ctl, &msgblk.recipients);
	else
	{
	    /*
	     * We haven't extracted the envelope address.
	     * So check all the "Resent-To" header addresses if 
	     * they exist.  If and only if they don't, consider
	     * the "To" adresses.
	     */
	    register struct addrblk *nextptr;
	    if (resent_to_addrchain) {
		/* delete the "To" chain and substitute it 
		 * with the "Resent-To" list 
		 */
		while (to_addrchain) {
		    nextptr = to_addrchain->next;
		    free(to_addrchain);
		    to_addrchain = nextptr;
		}
		to_addrchain = resent_to_addrchain;
		resent_to_addrchain = NULL;
	    }
	    /* now look for remaining adresses */
	    while (to_addrchain) {
		find_server_names(msgblk.headers+to_addrchain->offset, ctl, &msgblk.recipients);
		nextptr = to_addrchain->next;
		free(to_addrchain);
		to_addrchain = nextptr;
	    }
	}
	if (!accept_count)
	{
	    no_local_matches = TRUE;
	    save_str(&msgblk.recipients, run.postmaster, XMIT_ACCEPT);
	    if (outlevel >= O_DEBUG)
		report(stdout,
		      _("no local matches, forwarding to %s\n"),
		      run.postmaster);
	}
    }
    else	/* it's a single-drop box, use first localname */
	save_str(&msgblk.recipients, ctl->localnames->id, XMIT_ACCEPT);


    /*
     * Time to either address the message or decide we can't deliver it yet.
     */
    if (ctl->errcount > olderrs)	/* there were DNS errors above */
    {
	if (outlevel >= O_DEBUG)
	    report(stdout,
		   _("forwarding and deletion suppressed due to DNS errors\n"));
	free(msgblk.headers);
	free_str_list(&msgblk.recipients);
	return(PS_TRANSIENT);
    }
    else
    {
	/* set up stuffline() so we can deliver the message body through it */ 
	if ((n = open_sink(ctl, &msgblk,
			   &good_addresses, &bad_addresses)) != PS_SUCCESS)
	{
	    free(msgblk.headers);
	    free_str_list(&msgblk.recipients);
	    return(n);
	}
    }

    n = 0;
    /*
     * Some server/sendmail combinations cause problems when our
     * synthetic Received line is before the From header.  Cope
     * with this...
     */
    if ((rcv = strstr(msgblk.headers, "Received:")) == (char *)NULL)
	rcv = msgblk.headers;
    /* handle ">Received:" lines too */
    while (rcv > msgblk.headers && rcv[-1] != '\n')
	rcv--;
    if (rcv > msgblk.headers)
    {
	char	c = *rcv;

	*rcv = '\0';
	n = stuffline(ctl, msgblk.headers);
	*rcv = c;
    }
    if (!run.invisible && n != -1)
    {
	/* utter any per-message Received information we need here */
	sprintf(buf, "Received: from %s\r\n", ctl->server.truename);
	n = stuffline(ctl, buf);
	if (n != -1)
	{
	    /*
	     * This header is technically invalid under RFC822.
	     * POP3, IMAP, etc. are not legal mail-parameter values.
	     *
	     * We used to include ctl->remotename in this log line,
	     * but this can be secure information that would be bad
	     * to reveal.
	     */
	    sprintf(buf, "\tby %s with %s (fetchmail-%s)\r\n",
		    fetchmailhost,
		    protocol->name,
		    VERSION);
	    n = stuffline(ctl, buf);
	    if (n != -1)
	    {
		buf[0] = '\t';
		if (good_addresses == 0)
		{
		    sprintf(buf+1, 
			    "for %s@%s (by default); ",
			    user, ctl->destaddr);
		}
		else if (good_addresses == 1)
		{
		    for (idp = msgblk.recipients; idp; idp = idp->next)
			if (idp->val.status.mark == XMIT_ACCEPT)
			    break;	/* only report first address */
		    if (strchr(idp->id, '@'))
			sprintf(buf+1, "for %s", idp->id);
		    else
			/*
			 * This could be a bit misleading, as destaddr is
			 * the forwarding host rather than the actual 
			 * destination.  Most of the time they coincide.
			 */
			sprintf(buf+1, "for %s@%s", idp->id, ctl->destaddr);
		    sprintf(buf+strlen(buf), " (%s); ",
			    MULTIDROP(ctl) ? "multi-drop" : "single-drop");
		}
		else
		    buf[1] = '\0';

		strcat(buf, rfc822timestamp());
		strcat(buf, "\r\n");
		n = stuffline(ctl, buf);
	    }
	}
    }

    if (n != -1)
	n = stuffline(ctl, rcv);	/* ship out rest of msgblk.headers */

    if (n == -1)
    {
	report(stdout, _("writing RFC822 msgblk.headers\n"));
	release_sink(ctl);
	free(msgblk.headers);
	free_str_list(&msgblk.recipients);
	return(PS_IOERR);
    }
    else if (!run.use_syslog && outlevel >= O_VERBOSE)
	fputs("#", stderr);

    /* write error notifications */
    if (no_local_matches || has_nuls || bad_addresses)
    {
	int	errlen = 0;
	char	errhd[USERNAMELEN + POPBUFSIZE], *errmsg;

	errmsg = errhd;
	(void) strcpy(errhd, "X-Fetchmail-Warning: ");
	if (no_local_matches)
	{
	    if (reject_count != 1)
		strcat(errhd, _("no recipient addresses matched declared local names"));
	    else
	    {
		for (idp = msgblk.recipients; idp; idp = idp->next)
		    if (idp->val.status.mark == XMIT_REJECT)
			break;
		sprintf(errhd+strlen(errhd), _("recipient address %s didn't match any local name"), idp->id);
	    }
	}

	if (has_nuls)
	{
	    if (errhd[sizeof("X-Fetchmail-Warning: ")])
		strcat(errhd, "; ");
	    strcat(errhd, _("message has embedded NULs"));
	}

	if (bad_addresses)
	{
	    if (errhd[sizeof("X-Fetchmail-Warning: ")])
		strcat(errhd, "; ");
	    strcat(errhd, _("SMTP listener rejected local recipient addresses: "));
	    errlen = strlen(errhd);
	    for (idp = msgblk.recipients; idp; idp = idp->next)
		if (idp->val.status.mark == XMIT_RCPTBAD)
		    errlen += strlen(idp->id) + 2;

	    xalloca(errmsg, char *, errlen+3);
	    (void) strcpy(errmsg, errhd);
	    for (idp = msgblk.recipients; idp; idp = idp->next)
		if (idp->val.status.mark == XMIT_RCPTBAD)
		{
		    strcat(errmsg, idp->id);
		    if (idp->next)
			strcat(errmsg, ", ");
		}

	}

	strcat(errmsg, "\r\n");

	/* ship out the error line */
	stuffline(ctl, errmsg);
    }

    /* issue the delimiter line */
    cp = buf;
    *cp++ = '\r';
    *cp++ = '\n';
    *cp++ = '\0';
    stuffline(ctl, buf);

    free(msgblk.headers);
    free_str_list(&msgblk.recipients);
    return(headers_ok ? PS_SUCCESS : PS_TRUNCATED);
}

static int readbody(int sock, struct query *ctl, flag forward, int len)
/* read and dispose of a message body presented on sock */
/*   ctl:		query control record */
/*   sock:		to which the server is connected */
/*   len:		length of message */
/*   forward:		TRUE to forward */
{
    int	linelen;
    unsigned char buf[MSGBUFSIZE+4];
    unsigned char *inbufp = buf;
    flag issoftline = FALSE;

    /*
     * Pass through the text lines in the body.
     *
     * Yes, this wants to be ||, not &&.  The problem is that in the most
     * important delimited protocol, POP3, the length is not reliable.
     * As usual, the problem is Microsoft brain damage; see FAQ item S2.
     * So, for delimited protocols we need to ignore the length here and
     * instead drop out of the loop with a break statement when we see
     * the message delimiter.
     */
    while (protocol->delimited || len > 0)
    {
	set_timeout(mytimeout);
	if ((linelen = SockRead(sock, inbufp, sizeof(buf)-4-(inbufp-buf)))==-1)
	{
	    set_timeout(0);
	    release_sink(ctl);
	    return(PS_SOCKET);
	}
	set_timeout(0);

	/* write the message size dots */
	if (linelen > 0)
	{
	    sizeticker += linelen;
	    while (sizeticker >= SIZETICKER)
	    {
		if (!run.use_syslog && outlevel > O_SILENT)
		{
		    fputc('.', stdout);
		    fflush(stdout);
		}
		sizeticker -= SIZETICKER;
	    }
	}
	len -= linelen;

	/* check for end of message */
	if (protocol->delimited && *inbufp == '.')
	    if (inbufp[1] == '\r' && inbufp[2] == '\n' && inbufp[3] == '\0')
		break;
	    else if (inbufp[1] == '\n' && inbufp[2] == '\0')
		break;
	    else
		msglen--;	/* subtract the size of the dot escape */

	msglen += linelen;

	if (ctl->mimedecode && (ctl->mimemsg & MSG_NEEDS_DECODE)) {
	    issoftline = UnMimeBodyline(&inbufp, protocol->delimited, issoftline);
	    if (issoftline && (sizeof(buf)-1-(inbufp-buf) < 200))
	    {
		/*
		 * Soft linebreak, but less than 200 bytes left in
		 * input buffer. Rather than doing a buffer overrun,
		 * ignore the soft linebreak, NL-terminate data and
		 * deliver what we have now.
		 * (Who writes lines longer than 2K anyway?)
		 */
		*inbufp = '\n'; *(inbufp+1) = '\0';
		issoftline = 0;
	    }
	}

	/* ship out the text line */
	if (forward && (!issoftline))
	{
	    int	n;
	    inbufp = buf;

	    /* guard against very long lines */
	    buf[MSGBUFSIZE+1] = '\r';
	    buf[MSGBUFSIZE+2] = '\n';
	    buf[MSGBUFSIZE+3] = '\0';

	    n = stuffline(ctl, buf);

	    if (n < 0)
	    {
		report(stdout, _("writing message text\n"));
		release_sink(ctl);
		return(PS_IOERR);
	    }
	    else if (outlevel >= O_VERBOSE)
		fputc('*', stderr);
	}
    }

    return(PS_SUCCESS);
}

#ifdef KERBEROS_V4
int
kerberos_auth (socket, canonical) 
/* authenticate to the server host using Kerberos V4 */
int socket;		/* socket to server host */
#if defined(__FreeBSD__) || defined(__OpenBSD__)
char *canonical;	/* server name */
#else
const char *canonical;	/* server name */
#endif
{
    char * host_primary;
    KTEXT ticket;
    MSG_DAT msg_data;
    CREDENTIALS cred;
    Key_schedule schedule;
    int rem;
  
    xalloca(ticket, KTEXT, sizeof (KTEXT_ST));
    rem = (krb_sendauth (0L, socket, ticket, "pop",
			 canonical,
			 ((char *) (krb_realmofhost (canonical))),
			 ((unsigned long) 0),
			 (&msg_data),
			 (&cred),
			 (schedule),
			 ((struct sockaddr_in *) 0),
			 ((struct sockaddr_in *) 0),
			 "KPOPV0.1"));
    if (rem != KSUCCESS)
    {
	report(stderr, _("kerberos error %s\n"), (krb_get_err_text (rem)));
	return (PS_AUTHFAIL);
    }
    return (0);
}
#endif /* KERBEROS_V4 */

#ifdef KERBEROS_V5
static int kerberos5_auth(socket, canonical)
/* authenticate to the server host using Kerberos V5 */
int socket;             /* socket to server host */
const char *canonical;  /* server name */
{
    krb5_error_code retval;
    krb5_context context;
    krb5_ccache ccdef;
    krb5_principal client = NULL, server = NULL;
    krb5_error *err_ret = NULL;

    krb5_auth_context auth_context = NULL;

    krb5_init_context(&context);
    krb5_init_ets(context);
    krb5_auth_con_init(context, &auth_context);

    if (retval = krb5_cc_default(context, &ccdef)) {
        report(stderr, "krb5_cc_default: %s\n", error_message(retval));
        return(PS_ERROR);
    }

    if (retval = krb5_cc_get_principal(context, ccdef, &client)) {
        report(stderr, "krb5_cc_get_principal: %s\n", error_message(retval));
        return(PS_ERROR);
    }

    if (retval = krb5_sname_to_principal(context, canonical, "pop",
           KRB5_NT_UNKNOWN,
           &server)) {
        report(stderr, "krb5_sname_to_principal: %s\n", error_message(retval));
        return(PS_ERROR);
    }

    retval = krb5_sendauth(context, &auth_context, (krb5_pointer) &socket,
         "KPOPV1.0", client, server,
         AP_OPTS_MUTUAL_REQUIRED,
         NULL,  /* no data to checksum */
         0,   /* no creds, use ccache instead */
         ccdef,
         &err_ret, 0,

         NULL); /* don't need reply */

    krb5_free_principal(context, server);
    krb5_free_principal(context, client);
    krb5_auth_con_free(context, auth_context);

    if (retval) {
#ifdef HEIMDAL
      if (err_ret && err_ret->e_text) {
          report(stderr, _("krb5_sendauth: %s [server says '%*s'] \n"),
                 error_message(retval),
                 err_ret->e_text);
#else
      if (err_ret && err_ret->text.length) {
          report(stderr, _("krb5_sendauth: %s [server says '%*s'] \n"),
		 error_message(retval),
		 err_ret->text.length,
		 err_ret->text.data);
#endif
	  krb5_free_error(context, err_ret);
      } else
          report(stderr, "krb5_sendauth: %s\n", error_message(retval));
      return(PS_ERROR);
    }

    return 0;
}
#endif /* KERBEROS_V5 */

static void clean_skipped_list(struct idlist **skipped_list)
/* struct "idlist" contains no "prev" ptr; we must remove unused items first */
{
    struct idlist *current=NULL, *prev=NULL, *tmp=NULL, *head=NULL;
    prev = current = head = *skipped_list;

    if (!head)
	return;
    do
    {
	/* if item has no reference, remove it */
	if (current && current->val.status.mark == 0)
	{
	    if (current == head) /* remove first item (head) */
	    {
		head = current->next;
		if (current->id) free(current->id);
		free(current);
		prev = current = head;
	    }
	    else /* remove middle/last item */
	    {
		tmp = current->next;
		prev->next = tmp;
		if (current->id) free(current->id);
		free(current);
		current = tmp;
	    }
	}
	else /* skip this item */
	{
	    prev = current;
	    current = current->next;
	}
    } while(current);

    *skipped_list = head;
}

static void send_size_warnings(struct query *ctl)
/* send warning mail with skipped msg; reset msg count when user notified */
{
    int size, nbr;
    int msg_to_send = FALSE;
    struct idlist *head=NULL, *current=NULL;
    int max_warning_poll_count;
#define OVERHD	"Subject: Fetchmail oversized-messages warning.\r\n\r\nThe following oversized messages remain on the mail server %s:"

    head = ctl->skipped;
    if (!head)
	return;

    /* don't start a notification message unless we need to */
    for (current = head; current; current = current->next)
	if (current->val.status.num == 0 && current->val.status.mark)
	    msg_to_send = TRUE;
    if (!msg_to_send)
	return;

    /*
     * There's no good way to recover if we can't send notification mail, 
     * but it's not a disaster, either, since the skipped mail will not
     * be deleted.
     */
    if (open_warning_by_mail(ctl, (struct msgblk *)NULL))
	return;
    stuff_warning(ctl, OVERHD, ctl->server.pollname);
 
    if (run.poll_interval == 0)
	max_warning_poll_count = 0;
    else
	max_warning_poll_count = ctl->warnings/run.poll_interval;

    /* parse list of skipped msg, adding items to the mail */
    for (current = head; current; current = current->next)
    {
	if (current->val.status.num == 0 && current->val.status.mark)
	{
	    nbr = current->val.status.mark;
	    size = atoi(current->id);
	    stuff_warning(ctl, 
		    _("\t%d msg %d octets long skipped by fetchmail.\n"),
		    nbr, size);
	}
	current->val.status.num++;
	current->val.status.mark = 0;

	if (current->val.status.num >= max_warning_poll_count)
	    current->val.status.num = 0;
    }

    close_warning_by_mail(ctl, (struct msgblk *)NULL);
#undef OVERHD
}

static int do_session(ctl, proto, maxfetch)
/* retrieve messages from server using given protocol method table */
struct query *ctl;		/* parsed options with merged-in defaults */
const struct method *proto;	/* protocol method table */
const int maxfetch;		/* maximum number of messages to fetch */
{
    int ok, js;
#ifdef HAVE_VOLATILE
    volatile int mailserver_socket = -1;	/* pacifies -Wall */
#else
    int mailserver_socket = -1;
#endif /* HAVE_VOLATILE */
    const char *msg;
    void (*pipesave)(int);
    void (*alrmsave)(int);
    struct idlist *current=NULL, *tmp=NULL;

    protocol = proto;
    ctl->server.base_protocol = protocol;

    pass = 0;
    tagnum = 0;
    tag[0] = '\0';	/* nuke any tag hanging out from previous query */
    ok = 0;

    /* set up the server-nonresponse timeout */
    alrmsave = signal(SIGALRM, timeout_handler);
    mytimeout = ctl->server.timeout;

    /* set up the broken-pipe timeout */
    pipesave = signal(SIGPIPE, sigpipe_handler);

    if ((js = setjmp(restart)))
    {
#ifdef HAVE_SIGPROCMASK
	/*
	 * Don't rely on setjmp() to restore the blocked-signal mask.
	 * It does this under BSD but is required not to under POSIX.
	 *
	 * If your Unix doesn't have sigprocmask, better hope it has
	 * BSD-like behavior.  Otherwise you may see fetchmail get
	 * permanently wedged after a second timeout on a bad read,
	 * because alarm signals were blocked after the first.
	 */
	sigset_t	allsigs;

	sigfillset(&allsigs);
	sigprocmask(SIG_UNBLOCK, &allsigs, NULL);
#endif /* HAVE_SIGPROCMASK */

	if (js == THROW_SIGPIPE)
	{
	    report(stdout,
		   _("SIGPIPE thrown from an MDA or a stream socket error"));
	    ok = PS_SOCKET;
	}
	else if (js == THROW_TIMEOUT)
	{
	    if (phase == OPEN_WAIT)
		report(stdout,
		       _("timeout after %d seconds waiting to connect to server %s.\n"),
		       ctl->server.timeout, ctl->server.pollname);
	    else if (phase == SERVER_WAIT)
		report(stdout,
		       _("timeout after %d seconds waiting for server %s.\n"),
		       ctl->server.timeout, ctl->server.pollname);
	    else if (phase == FORWARDING_WAIT)
		report(stdout,
		       _("timeout after %d seconds waiting for %s.\n"),
		       ctl->server.timeout,
		       ctl->mda ? "MDA" : "SMTP");
	    else if (phase == LISTENER_WAIT)
		report(stdout,
		       _("timeout after %d seconds waiting for listener to respond.\n"));
	    else
		report(stdout, 
		       _("timeout after %d seconds.\n"), ctl->server.timeout);

	    /*
	     * If we've exceeded our threshold for consecutive timeouts, 
	     * try to notify the user, then mark the connection wedged.
	     */
	    if (timeoutcount > MAX_TIMEOUTS 
		&& !open_warning_by_mail(ctl, (struct msgblk *)NULL))
	    {
		stuff_warning(ctl,
			      _("Subject: fetchmail sees repeated timeouts\r\n"));
		stuff_warning(ctl,
			      _("Fetchmail saw more than %d timouts while attempting to get mail from %s@%s.\n"), 
			      MAX_TIMEOUTS,
			      ctl->remotename,
			      ctl->server.truename);
		stuff_warning(ctl, 
			      _("This could mean that your mailserver is stuck, or that your SMTP listener"));
		stuff_warning(ctl, 
			      _("is wedged, or that your mailbox file on the server has been corrupted by"));
		stuff_warning(ctl, 
			      _("a server error.  You can run `fetchmail -v -v' to diagnose the problem."));
		stuff_warning(ctl,
			      _("Fetchmail won't poll this mailbox again until you restart it."));
		close_warning_by_mail(ctl, (struct msgblk *)NULL);
		ctl->wedged = TRUE;
	    }

	    ok = PS_ERROR;
	}

	/* try to clean up all streams */
	release_sink(ctl);
	if (ctl->smtp_socket != -1)
	    close(ctl->smtp_socket);
	if (mailserver_socket != -1)
	    SockClose(mailserver_socket);
    }
    else
    {
	char buf[POPBUFSIZE+1], *realhost;
	int len, num, count, new, bytes, deletions = 0, *msgsizes = NULL;
#if INET6
	int fetches, dispatches, oldphase;
#else /* INET6 */
	int port, fetches, dispatches, oldphase;
#endif /* INET6 */
	struct idlist *idp;

	/* execute pre-initialization command, if any */
	if (ctl->preconnect && (ok = system(ctl->preconnect)))
	{
	    report(stderr, 
		   _("pre-connection command failed with status %d\n"), ok);
	    ok = PS_SYNTAX;
	    goto closeUp;
	}

	/* open a socket to the mail server */
	oldphase = phase;
	phase = OPEN_WAIT;
	set_timeout(mytimeout);
#if !INET6
	port = ctl->server.port ? ctl->server.port : protocol->port;
#endif /* !INET6 */
	realhost = ctl->server.via ? ctl->server.via : ctl->server.pollname;
#if INET6
	if ((mailserver_socket = SockOpen(realhost, 
			     ctl->server.service ? ctl->server.service : protocol->service,
			     ctl->server.netsec, ctl->server.plugin)) == -1)
#else /* INET6 */
	if ((mailserver_socket = SockOpen(realhost, port, NULL, ctl->server.plugin)) == -1)
#endif /* INET6 */
	{
#if !INET6
	    int err_no = errno;
#ifdef HAVE_RES_SEARCH
	    if (err_no != 0 && h_errno != 0)
		report(stderr, _("fetchmail: internal inconsistency\n"));
#endif
	    /*
	     * Avoid generating a bogus error every poll cycle when we're
	     * in daemon mode but the connection to the outside world
	     * is down.
	     */
	    if (err_no == EHOSTUNREACH && run.poll_interval)
	        goto ehostunreach;

	    report_build(stderr, _("fetchmail: %s connection to %s failed"), 
			 protocol->name, ctl->server.pollname);
#ifdef HAVE_RES_SEARCH
	    if (h_errno != 0)
	    {
		if (h_errno == HOST_NOT_FOUND)
		    report_complete(stderr, _(": host is unknown\n"));
		else if (h_errno == NO_ADDRESS)
		    report_complete(stderr, _(": name is valid but has no IP address\n"));
		else if (h_errno == NO_RECOVERY)
		    report_complete(stderr, _(": unrecoverable name server error\n"));
		else if (h_errno == TRY_AGAIN)
		    report_complete(stderr, _(": temporary name server error\n"));
		else
		    report_complete(stderr, _(": unknown DNS error %d\n"), h_errno);
	    }
	    else
#endif /* HAVE_RES_SEARCH */
		report_complete(stderr, ": %s\n", strerror(err_no));

	ehostunreach:
#endif /* INET6 */
	    ok = PS_SOCKET;
	    set_timeout(0);
	    phase = oldphase;
	    goto closeUp;
	}
	set_timeout(0);
	phase = oldphase;

#ifdef KERBEROS_V4
	if (ctl->server.preauthenticate == A_KERBEROS_V4)
	{
	    set_timeout(mytimeout);
	    ok = kerberos_auth(mailserver_socket, ctl->server.truename);
	    set_timeout(0);
 	    if (ok != 0)
		goto cleanUp;
	}
#endif /* KERBEROS_V4 */

#ifdef KERBEROS_V5
	if (ctl->server.preauthenticate == A_KERBEROS_V5)
	{
	    set_timeout(mytimeout);
	    ok = kerberos5_auth(mailserver_socket, ctl->server.truename);
	    set_timeout(0);
 	    if (ok != 0)
		goto cleanUp;
	}
#endif /* KERBEROS_V5 */

	/* accept greeting message from mail server */
	ok = (protocol->parse_response)(mailserver_socket, buf);
	if (ok != 0)
	    goto cleanUp;

	/* try to get authorized to fetch mail */
	if (protocol->getauth)
	{
	    if (protocol->password_canonify)
		(protocol->password_canonify)(shroud, ctl->password);
	    else
		strcpy(shroud, ctl->password);

	    ok = (protocol->getauth)(mailserver_socket, ctl, buf);
	    if (ok != 0)
	    {
		if (ok == PS_LOCKBUSY)
		    report(stderr, _("Lock-busy error on %s@%s\n"),
			  ctl->remotename,
			  ctl->server.truename);
		else
		{
		    if (ok == PS_ERROR)
			ok = PS_AUTHFAIL;
		    report(stderr, _("Authorization failure on %s@%s\n"), 
			  ctl->remotename,
			  ctl->server.truename);

		    /*
		     * If we're running in background, try to mail the
		     * calling user a heads-up about the authentication 
		     * failure the first time it happens.
		     */
		    if (run.poll_interval
			&& !ctl->wedged 
			&& !open_warning_by_mail(ctl, (struct msgblk *)NULL))
		    {
			stuff_warning(ctl,
			       _("Subject: fetchmail authentication failed\r\n"));
			stuff_warning(ctl,
				_("Fetchmail could not get mail from %s@%s."), 
				ctl->remotename,
				ctl->server.truename);
			stuff_warning(ctl, 
			       _("The attempt to get authorization failed."));
			stuff_warning(ctl, 
			       _("This probably means your password is invalid."));
			close_warning_by_mail(ctl, (struct msgblk *)NULL);
			ctl->wedged = TRUE;
		    }
		}
		goto cleanUp;
	    }
	}

	ctl->errcount = fetches = 0;

	/* now iterate over each folder selected */
	for (idp = ctl->mailboxes; idp; idp = idp->next)
	{
	    pass = 0;
	    do {
		dispatches = 0;
		++pass;

		if (outlevel >= O_DEBUG)
		    if (idp->id)
			report(stdout, _("selecting or re-polling folder %s\n"), idp->id);
		    else
			report(stdout, _("selecting or re-polling default folder\n"));

		/* compute # of messages and number of new messages waiting */
		ok = (protocol->getrange)(mailserver_socket, ctl, idp->id, &count, &new, &bytes);
		if (ok != 0)
		    goto cleanUp;

		/* show user how many messages we downloaded */
		if (idp->id)
		    (void) sprintf(buf, _("%s at %s (folder %s)"),
				   ctl->remotename, ctl->server.truename, idp->id);
		else
		    (void) sprintf(buf, _("%s at %s"),
				   ctl->remotename, ctl->server.truename);
		if (outlevel > O_SILENT)
		    if (count == -1)		/* only used for ETRN */
			report(stdout, _("Polling %s\n"), ctl->server.truename);
		    else if (count != 0)
		    {
			if (new != -1 && (count - new) > 0)
			    report_build(stdout, _("%d %s (%d seen) for %s"),
				  count, count > 1 ? _("messages") :
				                     _("message"),
				  count-new, buf);
			else
			    report_build(stdout, _("%d %s for %s"), 
				  count, count > 1 ? _("messages") :
				                     _("message"), buf);
			if (bytes == -1)
			    report_complete(stdout, ".\n");
			else
			    report_complete(stdout, _(" (%d octets).\n"), bytes);
		    }
		    else
		    {
			/* these are pointless in normal daemon mode */
			if (pass == 1 && (run.poll_interval == 0 || outlevel >= O_VERBOSE))
			    report(stdout, _("No mail for %s\n"), buf); 
		    }

		/* very important, this is where we leave the do loop */ 
		if (count == 0)
		    break;

		if (check_only)
		{
		    if (new == -1 || ctl->fetchall)
			new = count;
		    fetches = new;	/* set error status ccorrectly */
		    goto no_error;
		}
		else if (count > 0)
		{    
		    flag	force_retrieval;

		    /*
		     * What forces this code is that in POP2 and
		     * IMAP2bis you can't fetch a message without
		     * having it marked `seen'.  In POP3 and IMAP4, on the
		     * other hand, you can (peek_capable is set by 
		     * each driver module to convey this; it's not a
		     * method constant because of the difference between
		     * IMAP2bis and IMAP4, and because POP3 doesn't  peek
		     * if fetchall is on).
		     *
		     * The result of being unable to peek is that if there's
		     * any kind of transient error (DNS lookup failure, or
		     * sendmail refusing delivery due to process-table limits)
		     * the message will be marked "seen" on the server without
		     * having been delivered.  This is not a big problem if
		     * fetchmail is running in foreground, because the user
		     * will see a "skipped" message when it next runs and get
		     * clued in.
		     *
		     * But in daemon mode this leads to the message
		     * being silently ignored forever.  This is not
		     * acceptable.
		     *
		     * We compensate for this by checking the error
		     * count from the previous pass and forcing all
		     * messages to be considered new if it's nonzero.
		     */
		    force_retrieval = !peek_capable && (ctl->errcount > 0);

		    /* 
		     * We need the size of each message before it's
		     * loaded in order to pass via the ESMTP SIZE
		     * option.  If the protocol has a getsizes method,
		     * we presume this means it doesn't get reliable
		     * sizes from message fetch responses.
		     */
		    if (proto->getsizes)
		    {
			int	i;

			xalloca(msgsizes, int *, sizeof(int) * count);
			for (i = 0; i < count; i++)
			    msgsizes[i] = -1;

			ok = (proto->getsizes)(mailserver_socket, count, msgsizes);
			if (ok != 0)
			    goto cleanUp;

			if (bytes == -1)
			{
			    bytes = 0;
			    for (i = 0; i < count; i++)
				bytes += msgsizes[i];
			}
		    }

		    /* read, forward, and delete messages */
		    for (num = 1; num <= count; num++)
		    {
			flag toolarge = NUM_NONZERO(ctl->limit)
			    && msgsizes && (msgsizes[num-1] > ctl->limit);
			flag oldmsg = (!new) || (protocol->is_old && (protocol->is_old)(mailserver_socket,ctl,num));
			flag fetch_it = !toolarge 
			    && (ctl->fetchall || force_retrieval || !oldmsg);
			flag suppress_delete = FALSE;
			flag suppress_forward = FALSE;
			flag suppress_readbody = FALSE;
			flag retained = FALSE;

			/*
			 * This check copes with Post Office/NT's
			 * annoying habit of randomly prepending bogus
			 * LIST items of length -1.  Patrick Audley
			 * <paudley@pobox.com> tells us: LIST shows a
			 * size of -1, RETR and TOP return "-ERR
			 * System error - couldn't open message", and
			 * DELE succeeds but doesn't actually delete
			 * the message.
			 */
			if (msgsizes && msgsizes[num-1] == -1)
			{
			    if (outlevel >= O_VERBOSE)
				report(stdout, 
				      _("Skipping message %d, length -1\n"),
				      num);
			    continue;
			}

			/*
			 * We may want to reject this message if it's old
			 * or oversized, and we're not forcing retrieval.
			 */
			if (!fetch_it)
			{
			    if (outlevel > O_SILENT)
			    {
				report_build(stdout, _("skipping message %d"), num);
				if (toolarge && !check_only) 
				{
				    char size[32];
				    int cnt;

				    /* convert sz to string */
				    sprintf(size, "%d", msgsizes[num-1]);

				    /* build a list of skipped messages
				     * val.id = size of msg (string cnvt)
				     * val.status.num = warning_poll_count
				     * val.status.mask = nbr of msg this size
				     */

				    current = ctl->skipped;

				    /* initialise warning_poll_count to the
				     * current value so that all new msg will
				     * be included in the next mail
				     */
				    cnt = current? current->val.status.num : 0;

				    /* if entry exists, increment the count */
				    if (current && 
					str_in_list(&current, size, FALSE))
				    {
					for ( ; current; 
						current = current->next)
					{
					    if (strcmp(current->id, size) == 0)
					    {
					        current->val.status.mark++;
						break;
					    }
					}
				    }
				    /* otherwise, create a new entry */
				    /* initialise with current poll count */
				    else
				    {
					tmp = save_str(&ctl->skipped, size, 1);
					tmp->val.status.num = cnt;
				    }

				    report_build(stdout, _(" (oversized, %d octets)"),
						msgsizes[num-1]);
				}
			    }
			}
			else
			{
			    flag wholesize = !protocol->fetch_body;

			    /* request a message */
			    ok = (protocol->fetch_headers)(mailserver_socket,ctl,num, &len);
			    if (ok != 0)
				goto cleanUp;

			    /* -1 means we didn't see a size in the response */
			    if (len == -1 && msgsizes)
			    {
				len = msgsizes[num - 1];
				wholesize = TRUE;
			    }

			    if (outlevel > O_SILENT)
			    {
				report_build(stdout, _("reading message %d of %d"),
					    num,count);

				if (len > 0)
				    report_build(stdout, _(" (%d %soctets)"),
					len, wholesize ? "" : _("header "));
				if (outlevel >= O_VERBOSE)
				    report_complete(stdout, "\n");
				else
				    report_complete(stdout, " ");
			    }

			    /* 
			     * Read the message headers and ship them to the
			     * output sink.  
			     */
			    ok = readheaders(mailserver_socket, len, msgsizes[num-1],
					     ctl, num);
			    if (ok == PS_RETAINED)
				suppress_forward = retained = TRUE;
			    else if (ok == PS_TRANSIENT)
				suppress_delete = suppress_forward = TRUE;
			    else if (ok == PS_REFUSED)
				suppress_forward = TRUE;
			    else if (ok == PS_TRUNCATED)
				suppress_readbody = TRUE;
			    else if (ok)
				goto cleanUp;

			    /* 
			     * If we're using IMAP4 or something else that
			     * can fetch headers separately from bodies,
			     * it's time to request the body now.  This
			     * fetch may be skipped if we got an anti-spam
			     * or other PS_REFUSED error response during
			     * readheaders.
			     */
			    if (protocol->fetch_body && !suppress_readbody) 
			    {
				if (outlevel >= O_VERBOSE)
				{
				    fputc('\n', stdout);
				    fflush(stdout);
				}

				if ((ok = (protocol->trail)(mailserver_socket, ctl, num)))
				    goto cleanUp;
				len = 0;
				if (!suppress_forward)
				{
				    if ((ok=(protocol->fetch_body)(mailserver_socket,ctl,num,&len)))
					goto cleanUp;
				    if (outlevel > O_SILENT && !wholesize)
					report_complete(stdout,
					       _(" (%d body octets) "), len);
				}
			    }

			    /* process the body now */
			    if (len > 0)
			    {
			        if (suppress_readbody)
				{
				  /* When readheaders returns PS_TRUNCATED,
				     the body (which has no content
				     has already been read by readheaders,
				     so we say readbody returned PS_SUCCESS */
				  ok = PS_SUCCESS;
				}
				else
				{
				  ok = readbody(mailserver_socket,
					        ctl,
					        !suppress_forward,
					        len);
				}
			        if (ok == PS_TRANSIENT)
				    suppress_delete = suppress_forward = TRUE;
				else if (ok)
				    goto cleanUp;

				/* tell server we got it OK and resynchronize */
				if (protocol->trail)
				{
				    if (outlevel >= O_VERBOSE)
				    {
					fputc('\n', stdout);
					fflush(stdout);
				    }

				    ok = (protocol->trail)(mailserver_socket, ctl, num);
				    if (ok != 0)
					goto cleanUp;
				}
			    }

			    /* count # messages forwarded on this pass */
			    if (!suppress_forward)
				dispatches++;

			    /*
			     * Check to see if the numbers matched?
			     *
			     * Yes, some servers foo this up horribly.
			     * All IMAP servers seem to get it right, and
			     * so does Eudora QPOP at least in 2.xx
			     * versions.
			     *
			     * Microsoft Exchange gets it completely
			     * wrong, reporting compressed rather than
			     * actual sizes (so the actual length of
			     * message is longer than the reported size).
			     * Another fine example of Microsoft brain death!
			     *
			     * Some older POP servers, like the old UCB
			     * POP server and the pre-QPOP QUALCOMM
			     * versions, report a longer size in the LIST
			     * response than actually gets shipped up.
			     * It's unclear what is going on here, as the
			     * QUALCOMM server (at least) seems to be
			     * reporting the on-disk size correctly.
			     */
			    if (msgsizes && msglen != msgsizes[num-1])
			    {
				if (outlevel >= O_DEBUG)
				    report(stdout,
					  _("message %d was not the expected length (%d actual != %d expected)\n"),
					  num, msglen, msgsizes[num-1]);
			    }

			    /* end-of-message processing starts here */
			    if (!close_sink(ctl, &msgblk, !suppress_forward))
			    {
				ctl->errcount++;
				suppress_delete = TRUE;
			    }
			    fetches++;
			}

			/*
			 * At this point in flow of control, either
			 * we've bombed on a protocol error or had
			 * delivery refused by the SMTP server
			 * (unlikely -- I've never seen it) or we've
			 * seen `accepted for delivery' and the
			 * message is shipped.  It's safe to mark the
			 * message seen and delete it on the server
			 * now.
			 */

			/* tell the UID code we've seen this */
			if (ctl->newsaved)
			{
			    struct idlist	*sdp;

			    for (sdp = ctl->newsaved; sdp; sdp = sdp->next)
				if (sdp->val.status.num == num)
				    sdp->val.status.mark = UID_SEEN;
			}

			/* maybe we delete this message now? */
			if (retained)
			{
			    if (outlevel > O_SILENT) 
				report(stdout, _(" retained\n"));
			}
			else if (protocol->delete
				 && !suppress_delete
				 && (fetch_it ? !ctl->keep : ctl->flush))
			{
			    deletions++;
			    if (outlevel > O_SILENT) 
				report_complete(stdout, _(" flushed\n"));
			    ok = (protocol->delete)(mailserver_socket, ctl, num);
			    if (ok != 0)
				goto cleanUp;
#ifdef POP3_ENABLE
			    delete_str(&ctl->newsaved, num);
#endif /* POP3_ENABLE */
			}
			else if (outlevel > O_SILENT) 
			    report_complete(stdout, _(" not flushed\n"));

			/* perhaps this as many as we're ready to handle */
			if (maxfetch && maxfetch <= fetches && fetches < count)
			{
			    report(stdout, _("fetchlimit %d reached; %d messages left on server\n"),
				  maxfetch, count - fetches);
			    ok = PS_MAXFETCH;
			    goto cleanUp;
			}
		    }

		    if (!check_only && ctl->skipped
			&& run.poll_interval > 0 && !nodetach)
		    {
			clean_skipped_list(&ctl->skipped);
			send_size_warnings(ctl);
		    }
		}
	    } while
		  /*
		   * Only re-poll if we had some actual forwards, allowed
		   * deletions and had no errors.
		   * Otherwise it is far too easy to get into infinite loops.
		   */
		  (dispatches && protocol->retry && !ctl->keep && !ctl->errcount);
	}

   no_error:
	/* ordinary termination with no errors -- officially log out */
	ok = (protocol->logout_cmd)(mailserver_socket, ctl);
	/*
	 * Hmmmm...arguably this would be incorrect if we had fetches but
	 * no dispatches (due to oversized messages, etc.)
	 */
	if (ok == 0)
	    ok = (fetches > 0) ? PS_SUCCESS : PS_NOMAIL;
	SockClose(mailserver_socket);
	goto closeUp;

    cleanUp:
	/* we only get here on error */
	if (ok != 0 && ok != PS_SOCKET)
	    (protocol->logout_cmd)(mailserver_socket, ctl);
	SockClose(mailserver_socket);
    }

    msg = (const char *)NULL;	/* sacrifice to -Wall */
    switch (ok)
    {
    case PS_SOCKET:
	msg = _("socket");
	break;
    case PS_AUTHFAIL:
	msg = _("authorization");
	break;
    case PS_SYNTAX:
	msg = _("missing or bad RFC822 header");
	break;
    case PS_IOERR:
	msg = _("MDA");
	break;
    case PS_ERROR:
	msg = _("client/server synchronization");
	break;
    case PS_PROTOCOL:
	msg = _("client/server protocol");
	break;
    case PS_LOCKBUSY:
	msg = _("lock busy on server");
	break;
    case PS_SMTP:
	msg = _("SMTP transaction");
	break;
    case PS_DNS:
	msg = _("DNS lookup");
	break;
    case PS_UNDEFINED:
	report(stderr, _("undefined error\n"));
	break;
    }
    /* no report on PS_MAXFETCH or PS_UNDEFINED */
    if (ok==PS_SOCKET || ok==PS_AUTHFAIL || ok==PS_SYNTAX 
		|| ok==PS_IOERR || ok==PS_ERROR || ok==PS_PROTOCOL 
		|| ok==PS_LOCKBUSY || ok==PS_SMTP || ok==PS_DNS)
	report(stderr, _("%s error while fetching from %s\n"), msg, ctl->server.pollname);

closeUp:
    /* execute post-initialization command, if any */
    if (ctl->postconnect && (ok = system(ctl->postconnect)))
    {
	report(stderr, _("post-connection command failed with status %d\n"), ok);
	if (ok == PS_SUCCESS)
	    ok = PS_SYNTAX;
    }

    signal(SIGALRM, alrmsave);
    signal(SIGPIPE, pipesave);
    return(ok);
}

int do_protocol(ctl, proto)
/* retrieve messages from server using given protocol method table */
struct query *ctl;		/* parsed options with merged-in defaults */
const struct method *proto;	/* protocol method table */
{
    int	ok;

#ifndef KERBEROS_V4
    if (ctl->server.preauthenticate == A_KERBEROS_V4)
    {
	report(stderr, _("Kerberos V4 support not linked.\n"));
	return(PS_ERROR);
    }
#endif /* KERBEROS_V4 */

#ifndef KERBEROS_V5
    if (ctl->server.preauthenticate == A_KERBEROS_V5)
    {
	report(stderr, _("Kerberos V5 support not linked.\n"));
	return(PS_ERROR);
    }
#endif /* KERBEROS_V5 */

    /* lacking methods, there are some options that may fail */
    if (!proto->is_old)
    {
	/* check for unsupported options */
	if (ctl->flush) {
	    report(stderr,
		    _("Option --flush is not supported with %s\n"),
		    proto->name);
	    return(PS_SYNTAX);
	}
	else if (ctl->fetchall) {
	    report(stderr,
		    _("Option --all is not supported with %s\n"),
		    proto->name);
	    return(PS_SYNTAX);
	}
    }
    if (!proto->getsizes && NUM_SPECIFIED(ctl->limit))
    {
	report(stderr,
		_("Option --limit is not supported with %s\n"),
		proto->name);
	return(PS_SYNTAX);
    }

    /*
     * If no expunge limit or we do expunges within the driver,
     * then just do one session, passing in any fetchlimit.
     */
    if (proto->retry || !NUM_SPECIFIED(ctl->expunge))
	return(do_session(ctl, proto, NUM_VALUE_OUT(ctl->fetchlimit)));
    /*
     * There's an expunge limit, and it isn't handled in the driver itself.
     * OK; do multiple sessions, each fetching a limited # of messages.
     * Stop if the total count of retrieved messages exceeds ctl->fetchlimit
     * (if it was nonzero).
     */
    else
    {
	int totalcount = 0; 
	int lockouts   = 0;
	int expunge    = NUM_VALUE_OUT(ctl->expunge);
	int fetchlimit = NUM_VALUE_OUT(ctl->fetchlimit);

	do {
	    ok = do_session(ctl, proto, expunge);
	    totalcount += expunge;
	    if (NUM_SPECIFIED(ctl->fetchlimit) && totalcount >= fetchlimit)
		break;
	    if (ok != PS_LOCKBUSY)
		lockouts = 0;
	    else if (lockouts >= MAX_LOCKOUTS)
		break;
	    else /* ok == PS_LOCKBUSY */
	    {
		/*
		 * Allow time for the server lock to release.  if we
		 * don't do this, we'll often hit a locked-mailbox
		 * condition and fail.
		 */
		lockouts++;
		sleep(3);
	    }
	} while
	    (ok == PS_MAXFETCH || ok == PS_LOCKBUSY);

	return(ok);
    }
}

#if defined(HAVE_STDARG_H)
void gen_send(int sock, const char *fmt, ... )
#else
void gen_send(sock, fmt, va_alist)
int sock;		/* socket to which server is connected */
const char *fmt;	/* printf-style format */
va_dcl
#endif
/* assemble command in printf(3) style and send to the server */
{
    char buf [MSGBUFSIZE+1];
    va_list ap;

    if (protocol->tagged)
	(void) sprintf(buf, "%s ", GENSYM);
    else
	buf[0] = '\0';

#if defined(HAVE_STDARG_H)
    va_start(ap, fmt) ;
#else
    va_start(ap);
#endif
#ifdef HAVE_VSNPRINTF
    vsnprintf(buf + strlen(buf), sizeof(buf), fmt, ap);
#else
    vsprintf(buf + strlen(buf), fmt, ap);
#endif
    va_end(ap);

    strcat(buf, "\r\n");
    SockWrite(sock, buf, strlen(buf));

    if (outlevel >= O_MONITOR)
    {
	char *cp;

	if (shroud && shroud[0] && (cp = strstr(buf, shroud)))
	{
	    char	*sp;

	    sp = cp + strlen(shroud);
	    *cp++ = '*';
	    while (*sp)
		*cp++ = *sp++;
	    *cp = '\0';
	}
	buf[strlen(buf)-2] = '\0';
	report(stdout, "%s> %s\n", protocol->name, buf);
    }
}

int gen_recv(sock, buf, size)
/* get one line of input from the server */
int sock;	/* socket to which server is connected */
char *buf;	/* buffer to receive input */
int size;	/* length of buffer */
{
    int oldphase = phase;	/* we don't have to be re-entrant */

    phase = SERVER_WAIT;
    set_timeout(mytimeout);
    if (SockRead(sock, buf, size) == -1)
    {
	set_timeout(0);
	phase = oldphase;
	return(PS_SOCKET);
    }
    else
    {
	set_timeout(0);
	if (buf[strlen(buf)-1] == '\n')
	    buf[strlen(buf)-1] = '\0';
	if (buf[strlen(buf)-1] == '\r')
	    buf[strlen(buf)-1] = '\0';
	if (outlevel >= O_MONITOR)
	    report(stdout, "%s< %s\n", protocol->name, buf);
	phase = oldphase;
	return(PS_SUCCESS);
    }
}

#if defined(HAVE_STDARG_H)
int gen_transact(int sock, const char *fmt, ... )
#else
int gen_transact(int sock, fmt, va_alist)
int sock;		/* socket to which server is connected */
const char *fmt;	/* printf-style format */
va_dcl
#endif
/* assemble command in printf(3) style, send to server, accept a response */
{
    int ok;
    char buf [MSGBUFSIZE+1];
    va_list ap;
    int oldphase = phase;	/* we don't have to be re-entrant */

    phase = SERVER_WAIT;

    if (protocol->tagged)
	(void) sprintf(buf, "%s ", GENSYM);
    else
	buf[0] = '\0';

#if defined(HAVE_STDARG_H)
    va_start(ap, fmt) ;
#else
    va_start(ap);
#endif
#ifdef HAVE_VSNPRINTF
    vsnprintf(buf + strlen(buf), sizeof(buf), fmt, ap);
#else
    vsprintf(buf + strlen(buf), fmt, ap);
#endif
    va_end(ap);

    strcat(buf, "\r\n");
    SockWrite(sock, buf, strlen(buf));

    if (outlevel >= O_MONITOR)
    {
	char *cp;

	if (shroud && shroud[0] && (cp = strstr(buf, shroud)))
	{
	    char	*sp;

	    sp = cp + strlen(shroud);
	    *cp++ = '*';
	    while (*sp)
		*cp++ = *sp++;
	    *cp = '\0';
	}
	buf[strlen(buf)-1] = '\0';
	report(stdout, "%s> %s\n", protocol->name, buf);
    }

    /* we presume this does its own response echoing */
    ok = (protocol->parse_response)(sock, buf);

    phase = oldphase;
    return(ok);
}

/* driver.c ends here */
