/*
 * sink.c -- forwarding/delivery support for fetchmail
 *
 * The interface of this module (open_sink(), stuff_line(), close_sink(),
 * release_sink()) seals off the delivery logic from the protocol machine,
 * so the latter won't have to care whether it's shipping to an [SL]MTP
 * listener daemon or an MDA pipe.
 *
 * Copyright 1998 by Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 */

#include  "config.h"
#include  <stdio.h>
#include  <errno.h>
#include  <string.h>
#include  <signal.h>
#include  <time.h>
#ifdef HAVE_MEMORY_H
#include  <memory.h>
#endif /* HAVE_MEMORY_H */
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#endif
#if defined(HAVE_UNISTD_H)
#include  <unistd.h>
#endif
#if defined(HAVE_STDARG_H)
#include  <stdarg.h>
#else
#include  <varargs.h>
#endif
#include  <ctype.h>
#include  <time.h>

#include  "fetchmail.h"
#include  "socket.h"
#include  "smtp.h"
#include  "i18n.h"

/* BSD portability hack...I know, this is an ugly place to put it */
#if !defined(SIGCHLD) && defined(SIGCLD)
#define SIGCHLD	SIGCLD
#endif

/* makes the open_sink()/close_sink() pair non-reentrant */
static int lmtp_responses;

int smtp_open(struct query *ctl)
/* try to open a socket to the appropriate SMTP server for this query */ 
{
    /* maybe it's time to close the socket in order to force delivery */
    if (NUM_NONZERO(ctl->batchlimit) && (ctl->smtp_socket != -1) && ++batchcount == ctl->batchlimit)
    {
	SockClose(ctl->smtp_socket);
	ctl->smtp_socket = -1;
	batchcount = 0;
    }

    /* if no socket to any SMTP host is already set up, try to open one */
    if (ctl->smtp_socket == -1) 
    {
	/* 
	 * RFC 1123 requires that the domain name in HELO address is a
	 * "valid principal domain name" for the client host. If we're
	 * running in invisible mode, violate this with malice
	 * aforethought in order to make the Received headers and
	 * logging look right.
	 *
	 * In fact this code relies on the RFC1123 requirement that the
	 * SMTP listener must accept messages even if verification of the
	 * HELO name fails (RFC1123 section 5.2.5, paragraph 2).
	 *
	 * How we compute the true mailhost name to pass to the
	 * listener doesn't affect behavior on RFC1123-violating
	 * listeners that check for name match; we're going to lose
	 * on those anyway because we can never give them a name
	 * that matches the local machine fetchmail is running on.
	 * What it will affect is the listener's logging.
	 */
	struct idlist	*idp;
	const char *id_me = run.invisible ? ctl->server.truename : fetchmailhost;
	int oldphase = phase;

	errno = 0;

	/*
	 * Run down the SMTP hunt list looking for a server that's up.
	 * Use both explicit hunt entries (value TRUE) and implicit 
	 * (default) ones (value FALSE).
	 */
	oldphase = phase;
	phase = LISTENER_WAIT;

	set_timeout(ctl->server.timeout);
	for (idp = ctl->smtphunt; idp; idp = idp->next)
	{
	    char	*cp, *parsed_host;
#ifdef INET6_ENABLE 
	    char	*portnum = SMTP_PORT;
#else
	    int		portnum = SMTP_PORT;
#endif /* INET6_ENABLE */

	    xalloca(parsed_host, char *, strlen(idp->id) + 1);

	    ctl->smtphost = idp->id;  /* remember last host tried. */
	    if(ctl->smtphost[0]=='/')
		ctl->listener = LMTP_MODE;

	    strcpy(parsed_host, idp->id);
	    if ((cp = strrchr(parsed_host, '/')))
	    {
		*cp++ = 0;
#ifdef INET6_ENABLE 
		portnum = cp;
#else
		portnum = atoi(cp);
#endif /* INET6_ENABLE */
	    }

	    if (ctl->smtphost[0]=='/'){
		if((ctl->smtp_socket = UnixOpen(ctl->smtphost))==-1)
		    continue;
	    } else
	    if ((ctl->smtp_socket = SockOpen(parsed_host,portnum,NULL,
					     ctl->server.plugout)) == -1)
		continue;

	    /* return immediately for ODMR */
	    if (ctl->server.protocol == P_ODMR)
               return(ctl->smtp_socket); /* success */

	    /* are we doing SMTP or LMTP? */
	    SMTP_setmode(ctl->listener);

	    /* first, probe for ESMTP */
	    if (SMTP_ok(ctl->smtp_socket) == SM_OK &&
		    SMTP_ehlo(ctl->smtp_socket, id_me,
			  &ctl->server.esmtp_options) == SM_OK)
	       break;  /* success */

	    /*
	     * RFC 1869 warns that some listeners hang up on a failed EHLO,
	     * so it's safest not to assume the socket will still be good.
	     */
	    SockClose(ctl->smtp_socket);
	    ctl->smtp_socket = -1;

	    /* if opening for ESMTP failed, try SMTP */
	    if ((ctl->smtp_socket = SockOpen(parsed_host,portnum,NULL,
					     ctl->server.plugout)) == -1)
		continue;

	    if (SMTP_ok(ctl->smtp_socket) == SM_OK && 
		    SMTP_helo(ctl->smtp_socket, id_me) == SM_OK)
		break;  /* success */

	    SockClose(ctl->smtp_socket);
	    ctl->smtp_socket = -1;
	}
	set_timeout(0);
	phase = oldphase;
    }

    /*
     * RFC 1123 requires that the domain name part of the
     * RCPT TO address be "canonicalized", that is a FQDN
     * or MX but not a CNAME.  Some listeners (like exim)
     * enforce this.  Now that we have the actual hostname,
     * compute what we should canonicalize with.
     */
    ctl->destaddr = ctl->smtpaddress ? ctl->smtpaddress : ( ctl->smtphost && ctl->smtphost[0] != '/' ? ctl->smtphost : "localhost");

    if (outlevel >= O_DEBUG && ctl->smtp_socket != -1)
	report(stdout, _("forwarding to %s\n"), ctl->smtphost);

    return(ctl->smtp_socket);
}

/* these are shared by open_sink and stuffline */
static FILE *sinkfp;

int stuffline(struct query *ctl, char *buf)
/* ship a line to the given control block's output sink (SMTP server or MDA) */
{
    int	n, oldphase;
    char *last;

    /* The line may contain NUL characters. Find the last char to use
     * -- the real line termination is the sequence "\n\0".
     */
    last = buf;
    while ((last += strlen(last)) && (last[-1] != '\n'))
        last++;

    /* fix message lines that have only \n termination (for qmail) */
    if (ctl->forcecr)
    {
        if (last - 1 == buf || last[-2] != '\r')
	{
	    last[-1] = '\r';
	    *last++  = '\n';
	    *last    = '\0';
	}
    }

    oldphase = phase;
    phase = FORWARDING_WAIT;

    /*
     * SMTP byte-stuffing.  We only do this if the protocol does *not*
     * use .<CR><LF> as EOM.  If it does, the server will already have
     * decorated any . lines it sends back up.
     */
    if (*buf == '.')
    {
	if (ctl->server.base_protocol->delimited)	/* server has already byte-stuffed */
	{
	    if (ctl->mda)
		++buf;
	    else
		/* writing to SMTP, leave the byte-stuffing in place */;
	}
        else /* if (!protocol->delimited)	-- not byte-stuffed already */
	{
	    if (!ctl->mda)
		SockWrite(ctl->smtp_socket, buf, 1);	/* byte-stuff it */
	    else
		/* leave it alone */;
	}
    }

    /* we may need to strip carriage returns */
    if (ctl->stripcr)
    {
	char	*sp, *tp;

	for (sp = tp = buf; sp < last; sp++)
	    if (*sp != '\r')
		*tp++ =  *sp;
	*tp = '\0';
        last = tp;
    }

    n = 0;
    if (ctl->mda || ctl->bsmtp)
	n = fwrite(buf, 1, last - buf, sinkfp);
    else if (ctl->smtp_socket != -1)
	n = SockWrite(ctl->smtp_socket, buf, last - buf);

    phase = oldphase;

    return(n);
}

static void sanitize(char *s)
/* replace unsafe shellchars by an _ */
{
    const static char *ok_chars = " 1234567890!@%-_=+:,./abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char *cp;

    for (cp = s; *(cp += strspn(cp, ok_chars)); /* NO INCREMENT */)
    	*cp = '_';
}

static int send_bouncemail(struct query *ctl, struct msgblk *msg,
			   int userclass, char *message,
			   int nerrors, char *errors[])
/* bounce back an error report a la RFC 1892 */
{
    char daemon_name[18 + HOSTLEN] = "FETCHMAIL-DAEMON@";
    char boundary[BUFSIZ], *bounce_to;
    int sock;

    /* don't bounce in reply to undeliverable bounces */
    if (!msg->return_path[0] || strcmp(msg->return_path, "<>") == 0)
	return(FALSE);

    bounce_to = (run.bouncemail ? msg->return_path : run.postmaster);

    SMTP_setmode(SMTP_MODE);

    /* can't just use fetchmailhost here, it might be localhost */
    strcat(daemon_name, host_fqdn());

    /* we need only SMTP for this purpose */
    if ((sock = SockOpen("localhost", SMTP_PORT, NULL, NULL)) == -1
    		|| SMTP_ok(sock) != SM_OK 
		|| SMTP_helo(sock, fetchmailhost) != SM_OK
		|| SMTP_from(sock, daemon_name, (char *)NULL) != SM_OK
		|| SMTP_rcpt(sock, bounce_to) != SM_OK
		|| SMTP_data(sock) != SM_OK)
	return(FALSE);

    /* our first duty is to keep the sacred foo counters turning... */
#ifdef HAVE_SNPRINTF
    snprintf(boundary, sizeof(boundary),
#else
    sprintf(boundary,
#endif /* HAVE_SNPRINTF */
	    "foo-mani-padme-hum-%d-%d-%ld", 
	    (int)getpid(), (int)getppid(), time((time_t *)NULL));

    if (outlevel >= O_VERBOSE)
	report(stdout, _("SMTP: (bounce-message body)\n"));
    else
	/* this will usually go to sylog... */
	report(stderr, _("mail from %s bounced to %s\n"),
	       daemon_name, bounce_to);

    /* bouncemail headers */
    SockPrintf(sock, "Return-Path: <>\r\n");
    SockPrintf(sock, "From: %s\r\n", daemon_name);
    SockPrintf(sock, "To: %s\r\n", bounce_to);
    SockPrintf(sock, "MIME-Version: 1.0\r\n");
    SockPrintf(sock, "Content-Type: multipart/report; report-type=delivery-status;\r\n\tboundary=\"%s\"\r\n", boundary);
    SockPrintf(sock, "\r\n");

    /* RFC1892 part 1 -- human-readable message */
    SockPrintf(sock, "--%s\r\n", boundary); 
    SockPrintf(sock,"Content-Type: text/plain\r\n");
    SockPrintf(sock, "\r\n");
    SockWrite(sock, message, strlen(message));
    SockPrintf(sock, "\r\n");
    SockPrintf(sock, "\r\n");

    if (nerrors)
    {
	struct idlist	*idp;
	int		nusers;
	
	/* RFC1892 part 2 -- machine-readable responses */
	SockPrintf(sock, "--%s\r\n", boundary); 
	SockPrintf(sock,"Content-Type: message/delivery-status\r\n");
	SockPrintf(sock, "\r\n");
	SockPrintf(sock, "Reporting-MTA: dns; %s\r\n", fetchmailhost);

	nusers = 0;
	for (idp = msg->recipients; idp; idp = idp->next)
	    if (idp->val.status.mark == userclass)
	    {
		char	*error;
		/* Minimum RFC1894 compliance + Diagnostic-Code field */
		SockPrintf(sock, "\r\n");
		SockPrintf(sock, "Final-Recipient: rfc822; %s@%s\r\n", 
			   idp->id, fetchmailhost);
		SockPrintf(sock, "Last-Attempt-Date: %s\r\n", rfc822timestamp());
		SockPrintf(sock, "Action: failed\r\n");

		if (nerrors == 1)
		    /* one error applies to all users */
		    error = errors[0];
		else if (nerrors > nusers)
		{
		    SockPrintf(sock, "Internal error: SMTP error count doesn't match number of recipients.\r\n");
		    break;
		}
		else
		    /* errors correspond 1-1 to selected users */
		    error = errors[nusers++];
		
		if (strlen(error) > 9 && isdigit(error[4])
			&& error[5] == '.' && isdigit(error[6])
			&& error[7] == '.' && isdigit(error[8]))
		    /* Enhanced status code available, use it */
		    SockPrintf(sock, "Status: %5.5s\r\n", &(error[4]));
		else
		    /* Enhanced status code not available, fake one */
		    SockPrintf(sock, "Status: %c.0.0\r\n", error[0]);
		SockPrintf(sock, "Diagnostic-Code: %s\r\n", error);
	    }
	SockPrintf(sock, "\r\n");
    }

    /* RFC1892 part 3 -- headers of undelivered message */
    SockPrintf(sock, "--%s\r\n", boundary); 
    SockPrintf(sock, "Content-Type: text/rfc822-headers\r\n");
    SockPrintf(sock, "\r\n");
    SockWrite(sock, msg->headers, strlen(msg->headers));
    SockPrintf(sock, "\r\n");
    SockPrintf(sock, "--%s--\r\n", boundary); 

    if (SMTP_eom(sock) != SM_OK || SMTP_quit(sock))
	return(FALSE);

    SockClose(sock);

    return(TRUE);
}

static int handle_smtp_report(struct query *ctl, struct msgblk *msg)
/* handle SMTP errors based on the content of SMTP_response */
/* return of PS_REFUSED deletes mail from the server; PS_TRANSIENT keeps it */
{
    int smtperr = atoi(smtp_response);
    char *responses[1];

    xalloca(responses[0], char *, strlen(smtp_response)+1);
    strcpy(responses[0], smtp_response);

#ifdef __UNUSED__
    /*
     * Don't do this!  It can really mess you up if, for example, you're
     * reporting an error with a single RCPT TO address among several;
     * RSET discards the message body and it doesn't get sent to the
     * valid recipients.
     */
    SMTP_rset(ctl->smtp_socket);    /* stay on the safe side */
    if (outlevel >= O_DEBUG)
	report(stdout, _("Saved error is still %d\n"), smtperr);
#endif /* __UNUSED */

    /*
     * Note: send_bouncemail message strings are not made subject
     * to gettext translation because (a) they're going to be 
     * embedded in a text/plain 7bit part, and (b) they're
     * going to be associated with listener error-response
     * messages, which are probably in English (none of the
     * MTAs I know about are internationalized).
     */
    if (str_find(&ctl->antispam, smtperr))
    {
	/*
	 * SMTP listener explicitly refuses to deliver mail
	 * coming from this address, probably due to an
	 * anti-spam domain exclusion.  Respect this.  Don't
	 * try to ship the message, and don't prevent it from
	 * being deleted.  There's no point in bouncing the
	 * email either since most spammers don't put their
	 * real return email address anywhere in the headers
	 * (unless the user insists with the SET SPAMBOUNCE
	 * config option).
	 *
	 * Default values:
	 *
	 * 571 = sendmail's "unsolicited email refused"
	 * 550 = exim's new antispam response (temporary)
	 * 501 = exim's old antispam response
	 * 554 = Postfix antispam response.
	 *
	 */
	if (run.spambounce)
		send_bouncemail(ctl, msg, XMIT_ACCEPT,
			"Our spam filter rejected this transaction.\r\n", 
			1, responses);
	return(PS_REFUSED);
    }

    /*
     * Suppress error message only if the response specifically 
     * meant `excluded for policy reasons'.  We *should* see
     * an error when the return code is less specific.
     */
    if (smtperr >= 400)
	report(stderr, _("%cMTP error: %s\n"), 
	      ctl->listener,
	      responses[0]);

    switch (smtperr)
    {
    case 552: /* message exceeds fixed maximum message size */
	/*
	 * Permanent no-go condition on the
	 * ESMTP server.  Don't try to ship the message, 
	 * and allow it to be deleted.
	 */
	send_bouncemail(ctl, msg, XMIT_ACCEPT,
			"This message was too large (SMTP error 552).\r\n", 
			1, responses);
	return(run.bouncemail ? PS_REFUSED : PS_TRANSIENT);
  
    case 553: /* invalid sending domain */
	/*
	 * These latter days 553 usually means a spammer is trying to
	 * cover his tracks.  We never bouncemail on these, because 
	 * (a) the return address is invalid by definition, and 
	 * (b) we wouldn't want spammers to get confirmation that
	 * this address is live, anyway.
	 */
	send_bouncemail(ctl, msg, XMIT_ACCEPT,
			"Invalid address in MAIL FROM (SMTP error 553).\r\n", 
			1, responses);
	return(PS_REFUSED);

    default:
	/* bounce non-transient errors back to the sender */
	if (smtperr >= 500 && smtperr <= 599)
	    if (send_bouncemail(ctl, msg, XMIT_ACCEPT,
				"General SMTP/ESMTP error.\r\n", 
				1, responses))
		return(run.bouncemail ? PS_REFUSED : PS_TRANSIENT);
	/*
	 * We're going to end up here on 4xx errors, like:
	 *
	 * 451: temporarily unable to identify sender (exim)
	 * 452: temporary out-of-queue-space condition on the ESMTP server.
	 *
	 * These are temporary errors.  Don't try to ship the message,
	 * and suppress deletion so it can be retried on a future
	 * retrieval cycle.
	 *
	 * Bouncemail *might* be appropriate here as a delay
	 * notification (note; if we ever add this, we must make
	 * sure the RFC1894 Action field is "delayed" rather thwn
	 * "failed").  But it's not really necessary because
	 * these are not actual failures, we're very likely to be
	 * able to recover on the next cycle.
	 */
	return(PS_TRANSIENT);
    }
}

int open_sink(struct query *ctl, struct msgblk *msg,
	      int *good_addresses, int *bad_addresses)
/* set up sinkfp to be an input sink we can ship a message to */
{
    struct	idlist *idp;
#ifdef HAVE_SIGACTION
    struct      sigaction sa_new;
#endif /* HAVE_SIGACTION */

    *bad_addresses = *good_addresses = 0;

    if (ctl->bsmtp)		/* dump to a BSMTP batch file */
    {
	if (strcmp(ctl->bsmtp, "-") == 0)
	    sinkfp = stdout;
	else
	    sinkfp = fopen(ctl->bsmtp, "a");

	/* see the ap computation under the SMTP branch */
	fprintf(sinkfp, 
		"MAIL FROM: %s", (msg->return_path[0]) ? msg->return_path : user);

	if (ctl->pass8bits || (ctl->mimemsg & MSG_IS_8BIT))
	    fputs(" BODY=8BITMIME", sinkfp);
	else if (ctl->mimemsg & MSG_IS_7BIT)
	    fputs(" BODY=7BIT", sinkfp);

	/* exim's BSMTP processor does not handle SIZE */
	/* fprintf(sinkfp, " SIZE=%d", msg->reallen); */

	fprintf(sinkfp, "\r\n");

	/*
	 * RFC 1123 requires that the domain name part of the
	 * RCPT TO address be "canonicalized", that is a FQDN
	 * or MX but not a CNAME.  Some listeners (like exim)
	 * enforce this.  Now that we have the actual hostname,
	 * compute what we should canonicalize with.
	 */
	ctl->destaddr = ctl->smtpaddress ? ctl->smtpaddress : "localhost";

	*bad_addresses = 0;
	for (idp = msg->recipients; idp; idp = idp->next)
	    if (idp->val.status.mark == XMIT_ACCEPT)
	    {
	        if (ctl->smtpname)
 		    fprintf(sinkfp, "RCPT TO: %s\r\n", ctl->smtpname);
		else if (strchr(idp->id, '@'))
  		    fprintf(sinkfp,
			    "RCPT TO: %s\r\n", idp->id);
		else
		    fprintf(sinkfp,
			    "RCPT TO: %s@%s\r\n", idp->id, ctl->destaddr);
		*good_addresses = 0;
	    }

	fputs("DATA\r\n", sinkfp);

	if (ferror(sinkfp))
	{
	    report(stderr, _("BSMTP file open or preamble write failed\n"));
	    return(PS_BSMTP);
	}
    }

    /* 
     * Try to forward to an SMTP or LMTP listener.  If the attempt to 
     * open a socket fails, fall through to attempt delivery via
     * local MDA.
     */
    else if (!ctl->mda && smtp_open(ctl) != -1)
    {
	const char	*ap;
	char		options[MSGBUFSIZE]; 
	char		addr[HOSTLEN+USERNAMELEN+1];
	int		total_addresses;

	/*
	 * Compute ESMTP options.
	 */
	options[0] = '\0';
	if (ctl->server.esmtp_options & ESMTP_8BITMIME) {
             if (ctl->pass8bits || (ctl->mimemsg & MSG_IS_8BIT))
		strcpy(options, " BODY=8BITMIME");
             else if (ctl->mimemsg & MSG_IS_7BIT)
		strcpy(options, " BODY=7BIT");
        }

	if ((ctl->server.esmtp_options & ESMTP_SIZE) && msg->reallen > 0)
	    sprintf(options + strlen(options), " SIZE=%d", msg->reallen);

	/*
	 * Try to get the SMTP listener to take the Return-Path
	 * address as MAIL FROM.  If it won't, fall back on the
	 * remotename and mailserver host.  This won't affect replies,
	 * which use the header From address anyway; the MAIL FROM
	 * address is a place for the SMTP listener to send
	 * bouncemail.  The point is to guarantee a FQDN in the MAIL
	 * FROM line -- some SMTP listeners, like smail, become
	 * unhappy otherwise.
	 *
	 * RFC 1123 requires that the domain name part of the
	 * MAIL FROM address be "canonicalized", that is a
	 * FQDN or MX but not a CNAME.  We'll assume the Return-Path
	 * header is already in this form here (it certainly
	 * is if rewrite is on).  RFC 1123 is silent on whether
	 * a nonexistent hostname part is considered canonical.
	 *
	 * This is a potential problem if the MTAs further upstream
	 * didn't pass canonicalized From/Return-Path lines, *and* the
	 * local SMTP listener insists on them. 
         *
         * Handle the case where an upstream MTA is setting a return
         * path equal to "@".  Ghod knows why anyone does this, but 
	 * it's been reported to happen in mail from Amazon.com and
	 * Motorola.
	 */
	if (!msg->return_path[0] || (0 == strcmp(msg->return_path, "@")))
	{
#ifdef HAVE_SNPRINTF
	    snprintf(addr, sizeof(addr),
#else
	    sprintf(addr,
#endif /* HAVE_SNPRINTF */
		  "%s@%s", ctl->remotename, ctl->server.truename);
	    ap = addr;
	}
	else if (strchr(msg->return_path,'@') || strchr(msg->return_path,'!'))
	    ap = msg->return_path;
	else		/* in case Return-Path existed but was local */
	{
#ifdef HAVE_SNPRINTF
	    snprintf(addr, sizeof(addr),
#else
	    sprintf(addr,
#endif /* HAVE_SNPRINTF */
		    "%s@%s", msg->return_path, ctl->server.truename);
	    ap = addr;
	}

	if (SMTP_from(ctl->smtp_socket, ap, options) != SM_OK)
	{
	    int err = handle_smtp_report(ctl, msg);

	    SMTP_rset(ctl->smtp_socket);    /* stay on the safe side */
	    return(err);
	}

	/*
	 * Now list the recipient addressees
	 */
	total_addresses = 0;
	for (idp = msg->recipients; idp; idp = idp->next)
	    total_addresses++;
	for (idp = msg->recipients; idp; idp = idp->next)
	    if (idp->val.status.mark == XMIT_ACCEPT)
	    {
		if (strchr(idp->id, '@'))
		    strcpy(addr, idp->id);
		else {
		    if (ctl->smtpname) {
#ifdef HAVE_SNPRINTF
		        snprintf(addr, sizeof(addr)-1, "%s", ctl->smtpname);
#else
			sprintf(addr, "%s", ctl->smtpname);
#endif /* HAVE_SNPRINTF */

		    } else {
#ifdef HAVE_SNPRINTF
		      snprintf(addr, sizeof(addr)-1, "%s@%s", idp->id, ctl->destaddr);
#else
		      sprintf(addr, "%s@%s", idp->id, ctl->destaddr);
#endif /* HAVE_SNPRINTF */
		    }
		}
		if (SMTP_rcpt(ctl->smtp_socket, addr) == SM_OK)
		    (*good_addresses)++;
		else
		{
		    char	errbuf[POPBUFSIZE];

		    handle_smtp_report(ctl, msg);

		    (*bad_addresses)++;
		    idp->val.status.mark = XMIT_RCPTBAD;
		    if (outlevel >= O_VERBOSE)
			report(stderr, 
			      _("%cMTP listener doesn't like recipient address `%s'\n"),
			      ctl->listener, addr);
		}
	    }

	/*
	 * It's tempting to do local notification only if bouncemail was
	 * insufficient -- that is, to add && total_addresses > *bad_addresses
	 * to the test here.  The problem with this theory is that it would
	 * make initial diagnosis of a broken multidrop configuration very
	 * hard -- most single-recipient messages would just invisibly bounce.
	 */
	if (!(*good_addresses)) 
	{
	    if (!run.postmaster[0])
	    {
		if (outlevel >= O_VERBOSE)
		    report(stderr, _("no address matches; no postmaster set.\n"));
		SMTP_rset(ctl->smtp_socket);	/* required by RFC1870 */
		return(PS_REFUSED);
	    }
	    if (strchr(run.postmaster, '@'))
		strncpy(addr, run.postmaster, sizeof(addr));
	    else
	    {
#ifdef HAVE_SNPRINTF
		snprintf(addr, sizeof(addr)-1, "%s@%s", run.postmaster, ctl->destaddr);
#else
		sprintf(addr, "%s@%s", run.postmaster, ctl->destaddr);
#endif /* HAVE_SNPRINTF */
	    }

	    if (SMTP_rcpt(ctl->smtp_socket, addr) != SM_OK)
	    {
		report(stderr, _("can't even send to %s!\n"), run.postmaster);
		SMTP_rset(ctl->smtp_socket);	/* required by RFC1870 */
		return(PS_REFUSED);
	    }

	    if (outlevel >= O_VERBOSE)
		report(stderr, _("no address matches; forwarding to %s.\n"), run.postmaster);
	}

	/* 
	 * Tell the listener we're ready to send data.
	 * Some listeners (like zmailer) may return antispam errors here.
	 */
	if (SMTP_data(ctl->smtp_socket) != SM_OK)
	{
	    SMTP_rset(ctl->smtp_socket);    /* stay on the safe side */
	    return(handle_smtp_report(ctl, msg));
	}
    }

    /*
     * Awkward case.  User didn't specify an MDA.  Our attempt to get a
     * listener socket failed.  Try to cope anyway -- initial configuration
     * may have found procmail.
     */
    else if (!ctl->mda)
    {
	report(stderr, _("%cMTP connect to %s failed\n"),
	       ctl->listener,
	       ctl->smtphost ? ctl->smtphost : "localhost");

#ifndef FALLBACK_MDA
	/* No fallback MDA declared.  Bail out. */
	return(PS_SMTP);
#else
	/*
	 * If user had things set up to forward offsite, no way
	 * we want to deliver locally!
	 */
	if (ctl->smtphost && strcmp(ctl->smtphost, "localhost"))
	    return(PS_SMTP);

	/* 
	 * User was delivering locally.  We have a fallback MDA.
	 * Latch it in place, logging the error, and fall through.
	 */
	ctl->mda = FALLBACK_MDA;

	report(stderr, _("can't raise the listener; falling back to %s"),
			 FALLBACK_MDA);
#endif
    }

    if (ctl->mda)		/* must deliver through an MDA */
    {
	int	length = 0, fromlen = 0, nameslen = 0;
	char	*names = NULL, *before, *after, *from = NULL;

	ctl->destaddr = "localhost";

	for (idp = msg->recipients; idp; idp = idp->next)
	    if (idp->val.status.mark == XMIT_ACCEPT)
		(*good_addresses)++;

	length = strlen(ctl->mda);
	before = xstrdup(ctl->mda);

	/* get user addresses for %T (or %s for backward compatibility) */
	if (strstr(before, "%s") || strstr(before, "%T"))
	{
	    /*
	     * We go through this in order to be able to handle very
	     * long lists of users and (re)implement %s.
	     */
	    nameslen = 0;
	    for (idp = msg->recipients; idp; idp = idp->next)
		if ((idp->val.status.mark == XMIT_ACCEPT))
		    nameslen += (strlen(idp->id) + 1);	/* string + ' ' */
	    if ((*good_addresses == 0))
		nameslen = strlen(run.postmaster);

	    names = (char *)xmalloc(nameslen + 1);	/* account for '\0' */
	    if (*good_addresses == 0)
		strcpy(names, run.postmaster);
	    else
	    {
		names[0] = '\0';
		for (idp = msg->recipients; idp; idp = idp->next)
		    if (idp->val.status.mark == XMIT_ACCEPT)
		    {
			strcat(names, idp->id);
			strcat(names, " ");
		    }
		names[--nameslen] = '\0';	/* chop trailing space */
	    }

	    /* sanitize names in order to contain only harmless shell chars */
	    sanitize(names);
	}

	/* get From address for %F */
	if (strstr(before, "%F"))
	{
	    from = xstrdup(msg->return_path);

	    /* sanitize from in order to contain *only* harmless shell chars */
	    sanitize(from);

	    fromlen = strlen(from);
	}

	/* do we have to build an mda string? */
	if (names || from) 
	{		
	    char	*sp, *dp;

	    /* find length of resulting mda string */
	    sp = before;
	    while ((sp = strstr(sp, "%s"))) {
		length += nameslen - 2;	/* subtract %s */
		sp += 2;
	    }
	    sp = before;
	    while ((sp = strstr(sp, "%T"))) {
		length += nameslen - 2;	/* subtract %T */
		sp += 2;
	    }
	    sp = before;
	    while ((sp = strstr(sp, "%F"))) {
		length += fromlen - 2;	/* subtract %F */
		sp += 2;
	    }
		
	    after = xmalloc(length + 1);

	    /* copy mda source string to after, while expanding %[sTF] */
	    for (dp = after, sp = before; (*dp = *sp); dp++, sp++) {
		if (sp[0] != '%')	continue;

		/* need to expand? BTW, no here overflow, because in
		** the worst case (end of string) sp[1] == '\0' */
		if (sp[1] == 's' || sp[1] == 'T') {
		    strcpy(dp, names);
		    dp += nameslen;
		    sp++;	/* position sp over [sT] */
		    dp--;	/* adjust dp */
		} else if (sp[1] == 'F') {
		    strcpy(dp, from);
		    dp += fromlen;
		    sp++;	/* position sp over F */
		    dp--;	/* adjust dp */
		}
	    }

	    if (names) {
		free(names);
		names = NULL;
	    }
	    if (from) {
		free(from);
		from = NULL;
	    }

	    free(before);

	    before = after;
	}


	if (outlevel >= O_DEBUG)
	    report(stdout, _("about to deliver with: %s\n"), before);

#ifdef HAVE_SETEUID
	/*
	 * Arrange to run with user's permissions if we're root.
	 * This will initialize the ownership of any files the
	 * MDA creates properly.  (The seteuid call is available
	 * under all BSDs and Linux)
	 */
	seteuid(ctl->uid);
#endif /* HAVE_SETEUID */

	sinkfp = popen(before, "w");
	free(before);
	before = NULL;

#ifdef HAVE_SETEUID
	/* this will fail quietly if we didn't start as root */
	seteuid(0);
#endif /* HAVE_SETEUID */

	if (!sinkfp)
	{
	    report(stderr, _("MDA open failed\n"));
	    return(PS_IOERR);
	}

	/*
	 * We need to disable the normal SIGCHLD handling here because 
	 * sigchld_handler() would reap away the error status, returning
	 * error status instead of 0 for successful completion.
	 */
#ifndef HAVE_SIGACTION
	sigchld = signal(SIGCHLD, SIG_DFL);
#else
	memset (&sa_new, 0, sizeof sa_new);
	sigemptyset (&sa_new.sa_mask);
	sa_new.sa_handler = SIG_DFL;
	sigaction (SIGCHLD, &sa_new, NULL);
#endif /* HAVE_SIGACTION */
    }

    /*
     * We need to stash this away in order to know how many
     * response lines to expect after the LMTP end-of-message.
     */
    lmtp_responses = *good_addresses;

    return(PS_SUCCESS);
}

void release_sink(struct query *ctl)
/* release the per-message output sink, whether it's a pipe or SMTP socket */
{
    if (ctl->bsmtp && sinkfp)
	fclose(sinkfp);
    else if (ctl->mda)
    {
	if (sinkfp)
	{
	    pclose(sinkfp);
	    sinkfp = (FILE *)NULL;
	}
	deal_with_sigchld(); /* Restore SIGCHLD handling to reap zombies */
    }
}

int close_sink(struct query *ctl, struct msgblk *msg, flag forward)
/* perform end-of-message actions on the current output sink */
{
    if (ctl->mda)
    {
	int rc;

	/* close the delivery pipe, we'll reopen before next message */
	if (sinkfp)
	{
	    rc = pclose(sinkfp);
	    sinkfp = (FILE *)NULL;
	}
	else
	    rc = 0;

	deal_with_sigchld(); /* Restore SIGCHLD handling to reap zombies */

	if (rc)
	{
	    report(stderr, 
		   _("MDA returned nonzero status %d\n"), rc);
	    return(FALSE);
	}
    }
    else if (ctl->bsmtp && sinkfp)
    {
	int error;

	/* implicit disk-full check here... */
	fputs(".\r\n", sinkfp);
	error = ferror(sinkfp);
	if (strcmp(ctl->bsmtp, "-"))
	    if (fclose(sinkfp) == EOF) error = 1;
	if (error)
	{
	    report(stderr, 
		   _("Message termination or close of BSMTP file failed\n"));
	    return(FALSE);
	}
    }
    else if (forward)
    {
	/* write message terminator */
	if (SMTP_eom(ctl->smtp_socket) != SM_OK)
	{
	    if (handle_smtp_report(ctl, msg) != PS_REFUSED)
	    {
	        SMTP_rset(ctl->smtp_socket);    /* stay on the safe side */
		return(FALSE);
	    }
	    else
	    {
		report(stderr, _("SMTP listener refused delivery\n"));
	        SMTP_rset(ctl->smtp_socket);    /* stay on the safe side */
		return(TRUE);
	    }
	}

	/*
	 * If this is an SMTP connection, SMTP_eom() ate the response.
	 * But could be this is an LMTP connection, in which case we have to
	 * interpret either (a) a single 503 response meaning there
	 * were no successful RCPT TOs, or (b) a variable number of
	 * responses, one for each successful RCPT TO.  We need to send
	 * bouncemail on each failed response and then return TRUE anyway,
	 * otherwise the message will get left in the queue and resent
	 * to people who got it the first time.
	 */
	if (ctl->listener == LMTP_MODE)
	{
	    if (lmtp_responses == 0)
	    {
		SMTP_ok(ctl->smtp_socket); 

		/*
		 * According to RFC2033, 503 is the only legal response
		 * if no RCPT TO commands succeeded.  No error recovery
		 * is really possible here, as we have no idea what
		 * insane thing the listener might be doing if it doesn't
		 * comply.
		 */
		if (atoi(smtp_response) == 503)
		    report(stderr, _("LMTP delivery error on EOM\n"));
		else
		    report(stderr,
			  _("Unexpected non-503 response to LMTP EOM: %s\n"),
			  smtp_response);

		/*
		 * It's not completely clear what to do here.  We choose to
		 * interpret delivery failure here as a transient error, 
		 * the same way SMTP delivery failure is handled.  If we're
		 * wrong, an undead message will get stuck in the queue.
		 */
		return(FALSE);
	    }
	    else
	    {
		int	i, errors;
		char	**responses;

		/* eat the RFC2033-required responses, saving errors */ 
		xalloca(responses, char **, sizeof(char *) * lmtp_responses);
		for (errors = i = 0; i < lmtp_responses; i++)
		{
		    if (SMTP_ok(ctl->smtp_socket) == SM_OK)
			responses[i] = (char *)NULL;
		    else
		    {
			xalloca(responses[errors], 
				char *, 
				strlen(smtp_response)+1);
			strcpy(responses[errors], smtp_response);
			errors++;
		    }
		}

		if (errors == 0)
		    return(TRUE);	/* all deliveries succeeded */
		else
		    /*
		     * One or more deliveries failed.
		     * If we can bounce a failures list back to the
		     * sender, and the postmaster does not want to
		     * deal with the bounces return TRUE, deleting the
		     * message from the server so it won't be
		     * re-forwarded on subsequent poll cycles.
		     */
 		  return(send_bouncemail(ctl, msg, XMIT_ACCEPT,
					 "LSMTP partial delivery failure.\r\n",
					 errors, responses));
	    }
	}
    }

    return(TRUE);
}

int open_warning_by_mail(struct query *ctl, struct msgblk *msg)
/* set up output sink for a mailed warning to calling user */
{
    int	good, bad;

    /*
     * Dispatching warning email is a little complicated.  The problem is
     * that we have to deal with three distinct cases:
     *
     * 1. Single-drop running from user account.  Warning mail should
     * go to the local name for which we're collecting (coincides
     * with calling user).
     *
     * 2. Single-drop running from root or other privileged ID, with rc
     * file generated on the fly (Ken Estes's weird setup...)  Mail
     * should go to the local name for which we're collecting (does not 
     * coincide with calling user).
     * 
     * 3. Multidrop.  Mail must go to postmaster.  We leave the recipients
     * member null so this message will fall through to run.postmaster.
     *
     * The zero in the reallen element means we won't pass a SIZE
     * option to ESMTP; the message length would be more trouble than
     * it's worth to compute.
     */
    struct msgblk reply = {NULL, NULL, "FETCHMAIL-DAEMON@", 0};

    strcat(reply.return_path, fetchmailhost);

    if (!MULTIDROP(ctl))		/* send to calling user */
    {
	int status;

	save_str(&reply.recipients, ctl->localnames->id, XMIT_ACCEPT);
	status = open_sink(ctl, &reply, &good, &bad);
	free_str_list(&reply.recipients);
	return(status);
    }
    else				/* send to postmaster  */
	return(open_sink(ctl, &reply, &good, &bad));
}

#if defined(HAVE_STDARG_H)
void stuff_warning(struct query *ctl, const char *fmt, ... )
#else
void stuff_warning(struct query *ctl, fmt, va_alist)
struct query *ctl;
const char *fmt;	/* printf-style format */
va_dcl
#endif
/* format and ship a warning message line by mail */
{
    char	buf[POPBUFSIZE];
    va_list ap;

    /*
     * stuffline() requires its input to be writeable (for CR stripping),
     * so we needed to copy the message to a writeable buffer anyway in
     * case it was a string constant.  We make a virtue of that necessity
     * here by supporting stdargs/varargs.
     */
#if defined(HAVE_STDARG_H)
    va_start(ap, fmt) ;
#else
    va_start(ap);
#endif
#ifdef HAVE_VSNPRINTF
    vsnprintf(buf, sizeof(buf), fmt, ap);
#else
    vsprintf(buf, fmt, ap);
#endif
    va_end(ap);

#ifdef HAVE_SNPRINTF
    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\r\n");
#else
    strcat(buf, "\r\n");
#endif /* HAVE_SNPRINTF */

    stuffline(ctl, buf);
}

void close_warning_by_mail(struct query *ctl, struct msgblk *msg)
/* sign and send mailed warnings */
{
    stuff_warning(ctl, _("--\r\n\t\t\t\tThe Fetchmail Daemon\r\n"));
    close_sink(ctl, msg, TRUE);
}

/* sink.c ends here */
