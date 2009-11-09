/*
 * odmr.c -- ODMR protocol methods (see RFC 2645)
 *
 * For license terms, see the file COPYING in this directory.
 */

#include  "config.h"
#ifdef ODMR_ENABLE
#include  <stdio.h>
#include  <stdlib.h>
#include  <assert.h>
#ifdef HAVE_STRING_H /* strcat() */
#include <string.h>
#endif
#ifdef HAVE_NET_SOCKET_H /* BeOS needs this */
#include <net/socket.h>
#endif
#include  <sys/types.h>
#ifdef HAVE_NET_SELECT_H /* AIX needs this */
#include <net/select.h>
#endif
#ifdef HAVE_SYS_SELECT_H /* AIX 4.1, at least, needs this */
#include  <sys/select.h>
#endif
#include  <netdb.h>
#include  <errno.h>
#include  <unistd.h>
#include  "i18n.h"
#include  "fetchmail.h"
#include  "smtp.h"
#include  "socket.h"

static int odmr_ok (int sock, char *argbuf)
/* parse command response */
{
    int ok;

    (void)argbuf;
    ok = SMTP_ok(sock, SMTP_MODE, TIMEOUT_DEFAULT);
    if (ok == SM_UNRECOVERABLE)
	return(PS_PROTOCOL);
    else
	return(ok);
}

static int odmr_getrange(int sock, struct query *ctl, const char *id, 
			 int *countp, int *newp, int *bytes)
/* send ODMR and then run a reverse SMTP session */
{
    int ok, opts, smtp_sock;
    int doing_smtp_data = 0;   /* Are we in SMTP DATA state? */
    char buf [MSGBUFSIZE+1];
    struct idlist *qnp;		/* pointer to Q names */

    (void)id;
    if ((ok = SMTP_ehlo(sock, SMTP_MODE, fetchmailhost, 
			ctl->server.esmtp_name, ctl->server.esmtp_password,
			&opts)))
    {
	report(stderr, GT_("%s's SMTP listener does not support ESMTP\n"),
	      ctl->server.pollname);
	return(ok);
    }
    else if (!(opts & ESMTP_ATRN))
    {
	report(stderr, GT_("%s's SMTP listener does not support ATRN\n"),
	      ctl->server.pollname);
	return(PS_PROTOCOL);
    }

    /* make sure we don't enter the fetch loop */
    *bytes = *countp = *newp = -1;

    /* authenticate via CRAM-MD5 */
    ok = do_cram_md5(sock, "AUTH", ctl, "334 ");
    if (ok)
	return(ok);

    /*
     * By default, the hostlist has a single entry, the fetchmail host's
     * canonical DNS name.
     */
    buf[0] = '\0';
    for (qnp = ctl->domainlist; qnp; qnp = qnp->next)
	if (strlen(buf) + strlen(qnp->id) + 1 >= sizeof(buf))
	    break;
	else
	{
	    strcat(buf, qnp->id);
	    strcat(buf, ",");
	}
    buf[strlen(buf) - 1] = '\0';	/* nuke final comma */

    /* ship the domain list and get turnaround */
    gen_send(sock, "ATRN %s", buf);
    if ((ok = gen_recv(sock, buf, sizeof(buf))))
	return(ok);

    /* this switch includes all response codes described in RFC2645 */
    switch(atoi(buf))
    {
    case 250:	/* OK, turnaround is about to happe */
	if (outlevel > O_SILENT)
	    report(stdout, GT_("Turnaround now...\n"));
	break;

    case 450:	/* ATRN request refused */
	if (outlevel > O_SILENT)
	    report(stdout, GT_("ATRN request refused.\n"));
	return(PS_PROTOCOL);

    case 451:	/* Unable to process ATRN request now */
	report(stderr, GT_("Unable to process ATRN request now\n"));
	return(PS_EXCLUDE);

    case 453:	/* You have no mail */
	if (outlevel > O_SILENT)
	    report(stderr, GT_("You have no mail.\n"));
	return(PS_NOMAIL);

    case 502:	/* Command not implemented */
	report(stderr, GT_("Command not implemented\n"));
	return(PS_PROTOCOL);

    case 530:	/* Authentication required */
	report(stderr, GT_("Authentication required.\n"));
	return(PS_AUTHFAIL);

    default:
	report(stderr, GT_("Unknown ODMR error %d\n"), atoi(buf));
	return(PS_PROTOCOL);
    }

    /*
     * OK, if we got here it's time to become a pipe between the ODMR
     * remote server (sending) and the SMTP listener we've designated
     * (receiving).  We're not going to try to be a protocol machine;
     * instead, we'll use select(2) to watch the read sides of both
     * sockets and just throw their data at each other.
     */
    if ((smtp_sock = smtp_open(ctl)) == -1)
	return(PS_SOCKET);
    else
    {
	int	maxfd = (sock > smtp_sock) ? sock : smtp_sock;

	for (;;)
	{
	    fd_set	readfds;
	    struct timeval timeout;
	    char	buf[MSGBUFSIZE];

	    FD_ZERO(&readfds);
	    FD_SET(sock, &readfds);
	    FD_SET(smtp_sock, &readfds);

	    timeout.tv_sec  = ctl->server.timeout;
	    timeout.tv_usec = 0;

	    if (select(maxfd+1, &readfds, NULL, NULL, &timeout) == -1)
		return(PS_PROTOCOL);		/* timeout */

	    if (FD_ISSET(sock, &readfds))
	    {
		int n = SockRead(sock, buf, sizeof(buf));
		if (n <= 0)
		    break;

		SockWrite(smtp_sock, buf, n);
		if (outlevel >= O_MONITOR && !doing_smtp_data)
		    report(stdout, "ODMR< %s", buf);
	    }
	    if (FD_ISSET(smtp_sock, &readfds))
	    {
		int n = SockRead(smtp_sock, buf, sizeof(buf));
		if (n <= 0)
		    break;

		SockWrite(sock, buf, n);
		if (outlevel >= O_MONITOR)
		    report(stdout, "ODMR> %s", buf);

               /* We are about to receive message data if the local MTA
                * sends 354 (after receiving DATA) */
               if (!doing_smtp_data && !strncmp(buf, "354", 3))
               {
                   doing_smtp_data = 1;
		   if (outlevel > O_SILENT)
		       report(stdout, GT_("receiving message data\n"));
               }
               else if (doing_smtp_data)
                   doing_smtp_data = 0;
	    }
	}
	SockClose(smtp_sock);
    }

    return(0);
}

static int odmr_logout(int sock, struct query *ctl)
/* send logout command */
{
    /* if we have a smtp_socket, then we've turned around and the
       local smtp server is in control of the connection (so we don't
       send QUIT) */
    if (ctl->smtp_socket == -1)
       return(gen_transact(sock, "QUIT"));
    else
       return(PS_SUCCESS);
}

static const struct method odmr =
{
    "ODMR",		/* ODMR protocol */
    "odmr",		/* standard ODMR port */
    "odmrs",		/* ssl ODMR port */
    FALSE,		/* this is not a tagged protocol */
    FALSE,		/* this does not use a message delimiter */
    odmr_ok,		/* parse command response */
    NULL,		/* no need to get authentication */
    odmr_getrange,	/* initialize message sending */
    NULL,		/* we cannot get a list of sizes */
    NULL,		/* we cannot get a list of sizes of subsets */
    NULL,		/* how do we tell a message is old? */
    NULL,		/* no way to fetch headers */
    NULL,		/* no way to fetch body */
    NULL,		/* no message trailer */
    NULL,		/* how to delete a message */
    NULL,		/* how to mark a message as seen */
    NULL,		/* no mailbox support */
    odmr_logout,	/* log out, we're done */
    FALSE,		/* no, we can't re-poll */
};

int doODMR (struct query *ctl)
/* retrieve messages using ODMR */
{
    int status;

    if (ctl->keep) {
	fprintf(stderr, GT_("Option --keep is not supported with ODMR\n"));
	return(PS_SYNTAX);
    }
    if (ctl->flush) {
	fprintf(stderr, GT_("Option --flush is not supported with ODMR\n"));
	return(PS_SYNTAX);
    }
    if (ctl->mailboxes->id) {
	fprintf(stderr, GT_("Option --folder is not supported with ODMR\n"));
	return(PS_SYNTAX);
    }
    if (check_only) {
	fprintf(stderr, GT_("Option --check is not supported with ODMR\n"));
	return(PS_SYNTAX);
    }
    peek_capable = FALSE;

    status = do_protocol(ctl, &odmr);
    if (status == PS_NOMAIL)
	status = PS_SUCCESS;
    return(status);
}
#endif /* ODMR_ENABLE */

/* odmr.c ends here */
