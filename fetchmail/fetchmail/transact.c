/*
 * transact.c -- transaction primitives for the fetchmail driver loop
 *
 * Copyright 2001 by Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 *
 * 
 */

#include  "config.h"
#include  <stdio.h>
#include  <string.h>
#include  <ctype.h> /* isspace() */
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

#ifdef HAVE_NET_SOCKET_H
#include <net/socket.h>
#endif

#include "i18n.h"
#include "socket.h"
#include "fetchmail.h"

#ifndef strstr		/* glibc-2.1 declares this as a macro */
extern char *strstr();	/* needed on sysV68 R3V7.1. */
#endif /* strstr */

int mytimeout;		/* value of nonreponse timeout */
int suppress_tags;	/* emit tags? */
char shroud[PASSWORDLEN];	/* string to shroud in debug output */
struct msgblk msgblk;

char tag[TAGLEN];
static int tagnum;
#define GENSYM	(sprintf(tag, "A%04d", ++tagnum % TAGMOD), tag)

static int accept_count, reject_count;
static struct method *protocol;

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
	    report(stdout, GT_("mapped %s to local %s\n"), name, lname);
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
			    report(stdout, GT_("passed through %s matching %s\n"), 
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

/*
 * Return zero on a syntactically invalid address, nz on a valid one.
 *
 * This used to be strchr(a, '.'), but it turns out that lines like this
 *
 * Received: from punt-1.mail.demon.net by mailstore for markb@ordern.com
 *          id 938765929:10:27223:2; Fri, 01 Oct 99 08:18:49 GMT
 *
 * are not uncommon.  So now we just check that the following token is
 * not itself an email address.
 */
#define VALID_ADDRESS(a)	!strchr(a, '@')

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
	report(stdout, GT_("analyzing Received line:\n%s"), bufp);

    /* search for whitepace-surrounded "by" followed by valid address */
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

	    /* look for valid address */
	    if (VALID_ADDRESS(rbuf))
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
		      GT_("line accepted, %s is an alias of the mailserver\n"), rbuf);
	}
	else
	{
	    if (outlevel >= O_DEBUG)
		report(stdout, 
		      GT_("line rejected, %s is not an alias of the mailserver\n"), 
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
	    report(stdout, GT_("no Received address found\n"));
	return(NULL);
    }
    else
    {
	if (outlevel >= O_DEBUG) {
	    char *lf = rbuf + strlen(rbuf)-1;
	    *lf = '\0';
	    if (outlevel >= O_DEBUG)
		report(stdout, GT_("found Received address `%s'\n"), rbuf+2);
	    *lf = '\n';
	}
	return(rbuf);
    }
}

/* shared by readheaders and readbody */
static int sizeticker;

#define EMPTYLINE(s)	((s)[0] == '\r' && (s)[1] == '\n' && (s)[2] == '\0')

int readheaders(int sock,
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
    char		*received_for, *rcv, *cp, *delivered_to;
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

    /*
     * We used to free the header block unconditionally at the end of 
     * readheaders, but it turns out that if close_sink() hits an error
     * condition the code for sending bouncemail will actually look
     * at the freed storage and coredump...
     */
    if (msgblk.headers)
       free(msgblk.headers);

    /* initially, no message ID */
    if (ctl->thisid)
	free(ctl->thisid);
    ctl->thisid = NULL;

    msgblk.headers = received_for = delivered_to = NULL;
    from_offs = reply_to_offs = resent_from_offs = app_from_offs = 
	sender_offs = resent_sender_offs = env_offs = -1;
    oldlen = 0;
    msgblk.msglen = 0;
    skipcount = 0;
    ctl->mimemsg = 0;

    for (remaining = fetchlen; remaining > 0 || protocol->delimited; remaining -= linelen)
    {
	char *line;
	int overlong = FALSE;

	line = xmalloc(sizeof(buf));
	linelen = 0;
	line[0] = '\0';
	do {
	    set_timeout(mytimeout);
	    if ((n = SockRead(sock, buf, sizeof(buf)-1)) == -1) {
		set_timeout(0);
		free(line);
		free(msgblk.headers);
		msgblk.headers = NULL;
		return(PS_SOCKET);
	    }
	    set_timeout(0);
	    linelen += n;
	    msgblk.msglen += n;

		/*
		 * Try to gracefully handle the case, where the length of a
		 * line exceeds MSGBUFSIZE.
		 */
		if ( n && buf[n-1] != '\n' ) {
			unsigned int llen = strlen(line);
			overlong = TRUE;
			line = realloc(line, llen + n + 1);
			strcpy(line + llen, buf);
			ch = ' '; /* So the next iteration starts */
			continue;
		}

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
		if ( ctl->mimedecode && overlong ) {
			/*
			 * If we received an overlong line, we have to decode the
			 * whole line at once.
			 */
			line = (char *) realloc(line, strlen(line) + strlen(buf) +1);
			strcat(line, buf);
			UnMimeHeader(line);
		}
		else {
			if ( ctl->mimedecode )
				UnMimeHeader(buf);

			line = (char *) realloc(line, strlen(line) + strlen(buf) +1);
			strcat(line, buf);
		}

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
		has_nuls = (linelen != strlen(line));
		free(line);
		goto process_headers;
	    }

	    /*
	     * At least one brain-dead website (netmind.com) is known to
	     * send out robotmail that's missing the RFC822 delimiter blank
	     * line before the body! Without this check fetchmail segfaults.
	     * With it, we treat such messages as though they had the missing
	     * blank line.
	     */
	    if (!isspace(line[0]) && !strchr(line, ':'))
	    {
		headers_ok = FALSE;
		has_nuls = (linelen != strlen(line));
		free(line);
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
		if ((!run.use_syslog && !isafile(1)) || run.showdots)
		{
		    fputc('.', stdout);
		    fflush(stdout);
		}
		sizeticker -= SIZETICKER;
	    }
	}

	/* we see an ordinary (non-header, non-message-delimiter line */
	has_nuls = (linelen != strlen(line));

	/* save the message's ID, we may use it for killing duplicates later */
	if (MULTIDROP(ctl) && !strncasecmp(line, "Message-ID:", 11))
	    ctl->thisid = xstrdup(line);

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
#if INET6_ENABLE
	if (strncmp(protocol->service, "pop2", 4))
#else /* INET6_ENABLE */
	if (protocol->port != 109)
#endif /* INET6_ENABLE */
#endif /* POP2_ENABLE */
	    if (num == 1 && !strncasecmp(line, "X-IMAP:", 7)) {
		free(line);
		free(msgblk.headers);
		msgblk.headers = NULL;
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
	 * We remove all Delivered-To: headers.
	 * 
	 * This is to avoid false mail loops messages when delivering
	 * local messages to and from a Postfix/qmail mailserver. 
	 */
	if (ctl->dropdelivered && !strncasecmp(line, "Delivered-To:", 13)) 
	{
	    if (delivered_to)
		free(line);
	    else 
		delivered_to = line;
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
	    char *newhdrs;
	    int	newlen;

	    newlen = oldlen + strlen(line);
	    newhdrs = (char *) realloc(msgblk.headers, newlen + 1);
	    if (newhdrs == NULL) {
		free(line);
		return(PS_IOERR);
	    }
	    msgblk.headers = newhdrs;
	    strcpy(msgblk.headers + oldlen, line);
	    free(line);
	    line = msgblk.headers + oldlen;
	    oldlen = newlen;
	}

	/* find offsets of various special headers */
	if (!strncasecmp("From:", line, 5))
	    from_offs = (line - msgblk.headers);
	else if (!strncasecmp("Reply-To:", line, 9))
	    reply_to_offs = (line - msgblk.headers);
	else if (!strncasecmp("Resent-From:", line, 12))
	    resent_from_offs = (line - msgblk.headers);
	else if (!strncasecmp("Apparently-From:", line, 16))
	    app_from_offs = (line - msgblk.headers);
	/*
	 * Netscape 4.7 puts "Sender: zap" in mail headers.  Perverse...
	 *
	 * But a literal reading of RFC822 sec. 4.4.2 supports the idea
	 * that Sender: *doesn't* have to be a working email address.
	 *
	 * The definition of the Sender header in RFC822 says, in
	 * part, "The Sender mailbox specification includes a word
	 * sequence which must correspond to a specific agent (i.e., a
	 * human user or a computer program) rather than a standard
	 * address."  That implies that the contents of the Sender
	 * field don't need to be a legal email address at all So
	 * ignore any Sender or Resent-Semnder lines unless they
	 * contain @.
	 *
	 * (RFC2822 says the condents of Sender must be a valid mailbox
	 * address, which is also what RFC822 4.4.4 implies.)
	 */
	else if (!strncasecmp("Sender:", line, 7) && (strchr(line, '@') || strchr(line, '!')))
	    sender_offs = (line - msgblk.headers);
	else if (!strncasecmp("Resent-Sender:", line, 14) && (strchr(line, '@') || strchr(line, '!')))
	    resent_sender_offs = (line - msgblk.headers);

#ifdef __UNUSED__
 	else if (!strncasecmp("Message-Id:", line, 11))
	{
	    if (ctl->server.uidl)
 	    {
	        char id[IDLEN+1];

		line[IDLEN+12] = 0;		/* prevent stack overflow */
 		sscanf(line+12, "%s", id);
 	        if (!str_find( &ctl->newsaved, num))
		{
 		    struct idlist *new = save_str(&ctl->newsaved,id,UID_SEEN);
		    new->val.status.num = num;
		}
 	    }
 	}
#endif /* __UNUSED__ */

	/* if multidrop is on, gather addressee headers */
	if (MULTIDROP(ctl))
	{
	    if (!strncasecmp("To:", line, 3)
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
			if (skipcount++ < ctl->server.envskip)
			    continue;
			env_offs = (line - msgblk.headers);
		    }    
		}
		else if (!received_for && !strncasecmp("Received:", line, 9))
		{
		    if (skipcount++ < ctl->server.envskip)
			continue;
		    received_for = parse_received(ctl, line);
		}
	    }
	}
    }

 process_headers:    
    /*
     * When mail delivered to a multidrop mailbox on the server is
     * addressed to multiple people on the client machine, there will
     * be one copy left in the box for each recipient.  This is not a
     * problem if we have the actual recipient address to dispatch on
     * (e.g. because we've mined it out of sendmail trace headers, or
     * a qmail Delivered-To line, or a declared sender envelope line).
     *
     * But if we're mining addressees out of the To/Cc/Bcc fields, and
     * if the mail is addressed to N people, each recipient will
     * get N copies.  This is bad when N > 1.
     *
     * Foil this by suppressing all but one copy of a message with
     * a given Message-ID.  The accept_count test ensures that
     * multiple pieces of email with the same Message-ID, each
     * with a *single* addressee (the N == 1 case), won't be 
     * suppressed.
     *
     * Note: This implementation only catches runs of successive
     * messages with the same ID, but that should be good
     * enough. A more general implementation would have to store
     * ever-growing lists of seen message-IDs; in a long-running
     * daemon this would turn into a memory leak even if the 
     * implementation were perfect.
     * 
     * Don't mess with this code casually.  It would be way too easy
     * to break it in a way that blackholed mail.  Better to pass
     * the occasional duplicate than to do that...
     */
    if (!received_for && env_offs == -1 && !delivered_to)
    {
	if (ctl->lastid && ctl->thisid && !strcasecmp(ctl->lastid, ctl->thisid))
	{
	    if (accept_count > 1)
		return(PS_REFUSED);
	}
	else
	{
	    if (ctl->lastid)
		free(ctl->lastid);
	    ctl->lastid = ctl->thisid;
	    ctl->thisid = NULL;
	}
    }

    /*
     * We want to detect this early in case there are so few headers that the
     * dispatch logic barfs.
     */
    if (!headers_ok)
    {
	if (outlevel > O_SILENT)
	    report(stdout,
		   GT_("message delimiter found while scanning headers\n"));
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
	/* multi-line MAIL FROM addresses confuse SMTP terribly */
	if (ap && !strchr(ap, '\n')) 
	    strcpy(msgblk.return_path, ap);
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
	else if (delivered_to && ctl->server.envelope != STRING_DISABLED &&
      ctl->server.envelope && !strcasecmp(ctl->server.envelope, "Delivered-To"))
   {
	    find_server_names(delivered_to, ctl, &msgblk.recipients);
       free(delivered_to);
   }
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
	     * the "To" addresses.
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
		      GT_("no local matches, forwarding to %s\n"),
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
		   GT_("forwarding and deletion suppressed due to DNS errors\n"));
	free(msgblk.headers);
	msgblk.headers = NULL;
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
	    msgblk.headers = NULL;
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
        if (ctl->server.trueaddr) {
#ifdef HAVE_SNPRINTF
	    snprintf(buf, sizeof(buf),
#else
	    sprintf(buf, 
#endif /* HAVE_SNPRINTF */
		    "Received: from %s [%u.%u.%u.%u]\r\n", 
		    ctl->server.truename,
		    (unsigned char)ctl->server.trueaddr[0],
		    (unsigned char)ctl->server.trueaddr[1],
		    (unsigned char)ctl->server.trueaddr[2],
		    (unsigned char)ctl->server.trueaddr[3]);
	} else {
#ifdef HAVE_SNPRINTF
	  snprintf(buf, sizeof(buf),
#else                       
	  sprintf(buf,
#endif /* HAVE_SNPRINTF */
		  "Received: from %s\r\n", ctl->server.truename);
	}
	n = stuffline(ctl, buf);
	if (n != -1)
	{
	    /*
	     * This header is technically invalid under RFC822.
	     * POP3, IMAP, etc. are not legal mail-parameter values.
	     */
#ifdef HAVE_SNPRINTF
	    snprintf(buf, sizeof(buf),
#else
	    sprintf(buf,
#endif /* HAVE_SNPRINTF */
		    "\tby %s with %s (fetchmail-%s",
		    fetchmailhost,
		    protocol->name,
		    VERSION);
	    if (ctl->tracepolls)
	    {
		sprintf(buf + strlen(buf), " polling %s account %s",
			ctl->server.pollname, 
			ctl->remotename);
	    }
#ifdef HAVE_SNPRINTF
	    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), ")\r\n");
#else
	    strcat(buf, ")\r\n");
#endif /* HAVE_SNPRINTF */
	    n = stuffline(ctl, buf);
	    if (n != -1)
	    {
		buf[0] = '\t';
		if (good_addresses == 0)
		{
#ifdef HAVE_SNPRINTF
		    snprintf(buf+1, sizeof(buf)-1,
#else
		    sprintf(buf+1,
#endif /* HAVE_SNPRINTF */
			    "for %s@%s (by default); ",
			    user, ctl->destaddr);
		}
		else if (good_addresses == 1)
		{
		    for (idp = msgblk.recipients; idp; idp = idp->next)
			if (idp->val.status.mark == XMIT_ACCEPT)
			    break;	/* only report first address */
		    if (strchr(idp->id, '@'))
#ifdef HAVE_SNPRINTF
		    snprintf(buf+1, sizeof(buf)-1,
#else                       
		    sprintf(buf+1,
#endif /* HAVE_SNPRINTF */
			    "for %s", idp->id);
		    else
			/*
			 * This could be a bit misleading, as destaddr is
			 * the forwarding host rather than the actual 
			 * destination.  Most of the time they coincide.
			 */
#ifdef HAVE_SNPRINTF
		    	snprintf(buf+1, sizeof(buf)-1,
#else                       
			sprintf(buf+1,
#endif /* HAVE_SNPRINTF */
				"for %s@%s", idp->id, ctl->destaddr);
		    sprintf(buf+strlen(buf), " (%s); ",
			    MULTIDROP(ctl) ? "multi-drop" : "single-drop");
		}
		else
		    buf[1] = '\0';

#ifdef HAVE_SNPRINTF
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%s\r\n",
			rfc822timestamp());
#else
		strcat(buf, rfc822timestamp());
		strcat(buf, "\r\n");
#endif /* HAVE_SNPRINTF */
		n = stuffline(ctl, buf);
	    }
	}
    }

    if (n != -1)
	n = stuffline(ctl, rcv);	/* ship out rest of msgblk.headers */

    if (n == -1)
    {
	report(stdout, GT_("writing RFC822 msgblk.headers\n"));
	release_sink(ctl);
	free(msgblk.headers);
	msgblk.headers = NULL;
	free_str_list(&msgblk.recipients);
	return(PS_IOERR);
    }
    else if ((run.poll_interval == 0 || nodetach) && outlevel >= O_VERBOSE && !isafile(2))
	fputs("#", stdout);

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
		strcat(errhd, GT_("no recipient addresses matched declared local names"));
	    else
	    {
		for (idp = msgblk.recipients; idp; idp = idp->next)
		    if (idp->val.status.mark == XMIT_REJECT)
			break;
		sprintf(errhd+strlen(errhd), GT_("recipient address %s didn't match any local name"), idp->id);
	    }
	}

	if (has_nuls)
	{
	    if (errhd[sizeof("X-Fetchmail-Warning: ")])
		strcat(errhd, "; ");
	    strcat(errhd, GT_("message has embedded NULs"));
	}

	if (bad_addresses)
	{
	    if (errhd[sizeof("X-Fetchmail-Warning: ")])
		strcat(errhd, "; ");
	    strcat(errhd, GT_("SMTP listener rejected local recipient addresses: "));
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

/*    free(msgblk.headers); */
    free_str_list(&msgblk.recipients);
    return(headers_ok ? PS_SUCCESS : PS_TRUNCATED);
}

int readbody(int sock, struct query *ctl, flag forward, int len)
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
		if (outlevel > O_SILENT && (((run.poll_interval == 0 || nodetach) && !isafile(1)) || run.showdots))
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
	{
	    if (inbufp[1] == '\r' && inbufp[2] == '\n' && inbufp[3] == '\0')
		break;
	    else if (inbufp[1] == '\n' && inbufp[2] == '\0')
		break;
	    else
		msgblk.msglen--;	/* subtract the size of the dot escape */
	}

	msgblk.msglen += linelen;

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
		report(stdout, GT_("writing message text\n"));
		release_sink(ctl);
		return(PS_IOERR);
	    }
	    else if (outlevel >= O_VERBOSE && !isafile(1))
	    {
		fputc('*', stdout);
		fflush(stdout);
	    }
	}
    }

    return(PS_SUCCESS);
}

void init_transact(const struct method *proto)
/* initialize state for the send and receive functions */
{
    tagnum = 0;
    tag[0] = '\0';	/* nuke any tag hanging out from previous query */
    protocol = (struct method *)proto;
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

    if (protocol->tagged && !suppress_tags)
	(void) sprintf(buf, "%s ", GENSYM);
    else
	buf[0] = '\0';

#if defined(HAVE_STDARG_H)
    va_start(ap, fmt);
#else
    va_start(ap);
#endif
#ifdef HAVE_VSNPRINTF
    vsnprintf(buf + strlen(buf), sizeof(buf)-strlen(buf), fmt, ap);
#else
    vsprintf(buf + strlen(buf), fmt, ap);
#endif
    va_end(ap);

#ifdef HAVE_SNPRINTF
    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\r\n");
#else
    strcat(buf, "\r\n");
#endif /* HAVE_SNPRINTF */
    SockWrite(sock, buf, strlen(buf));

    if (outlevel >= O_MONITOR)
    {
	char *cp;

	if (shroud[0] && (cp = strstr(buf, shroud)))
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

    if (protocol->tagged && !suppress_tags)
	(void) sprintf(buf, "%s ", GENSYM);
    else
	buf[0] = '\0';

#if defined(HAVE_STDARG_H)
    va_start(ap, fmt) ;
#else
    va_start(ap);
#endif
#ifdef HAVE_VSNPRINTF
    vsnprintf(buf + strlen(buf), sizeof(buf)-strlen(buf), fmt, ap);
#else
    vsprintf(buf + strlen(buf), fmt, ap);
#endif
    va_end(ap);

#ifdef HAVE_SNPRINTF
    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\r\n");
#else
    strcat(buf, "\r\n");
#endif /* HAVE_SNPRINTF */
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

/* transact.c ends here */
