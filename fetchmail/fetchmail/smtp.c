/*
 * smtp.c -- code for speaking SMTP to a listener port
 *
 * Concept due to Harry Hochheiser.  Implementation by ESR.  Cleanup and
 * strict RFC821 compliance by Cameron MacPherson.
 *
 * Copyright 1997 Eric S. Raymond
 * For license terms, see the file COPYING in this directory.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "fetchmail.h"
#include "socket.h"
#include "smtp.h"
#include "config.h"
#include "i18n.h"

struct opt
{
    const char *name;
    int value;
};

static struct opt extensions[] =
{
    {"8BITMIME",	ESMTP_8BITMIME},
    {"SIZE",    	ESMTP_SIZE},
    {"ETRN",		ESMTP_ETRN},
    {"AUTH ",		ESMTP_AUTH},
#ifdef ODMR_ENABLE
    {"ATRN",		ESMTP_ATRN},
#endif /* ODMR_ENABLE */
    {(char *)NULL, 0},
};

char smtp_response[MSGBUFSIZE];

static char smtp_mode = 'S';

void SMTP_setmode(char sl)
/* set whether we are speaking SMTP or LMTP */
{
    smtp_mode = sl;
}

int SMTP_helo(int sock,const char *host)
/* send a "HELO" message to the SMTP listener */
{
  int ok;

  SockPrintf(sock,"HELO %s\r\n", host);
  if (outlevel >= O_MONITOR)
      report(stdout, "SMTP> HELO %s\n", host);
  ok = SMTP_ok(sock);
  return ok;
}

static void SMTP_auth_error(int sock, char *msg)
{
    SockPrintf(sock, "*\r\n");
    SockRead(sock, smtp_response, sizeof(smtp_response) - 1);
    if (outlevel >= O_MONITOR) report(stdout, msg);
}

static void SMTP_auth(int sock, char *username, char *password, char *buf)
/* ESMTP Authentication support for fetchmail by Wojciech Polak */
{	
	int c;
	char *p = 0;
	char b64buf[512];
	char tmp[512];

	if (!username || !password) return;

	memset(b64buf, 0, sizeof(b64buf));
	memset(tmp, 0, sizeof(tmp));

	if (strstr(buf, "CRAM-MD5")) {
		unsigned char digest[16];
		memset(digest, 0, sizeof(digest));

		if (outlevel >= O_MONITOR)
			report(stdout, GT_("ESMTP CRAM-MD5 Authentication...\n"));
		SockPrintf(sock, "AUTH CRAM-MD5\r\n");
		SockRead(sock, smtp_response, sizeof(smtp_response) - 1);
		strncpy(tmp, smtp_response, sizeof(tmp));
		tmp[sizeof(tmp)-1] = '\0';

		if (strncmp(tmp, "334 ", 4)) { /* Server rejects AUTH */
			SMTP_auth_error(sock, GT_("Server rejected the AUTH command.\n"));
			return;
		}

		p = strchr(tmp, ' ');
		p++;
		/* (hmh) from64tobits will not NULL-terminate strings! */
		if (from64tobits(b64buf, p, sizeof(b64buf) - 1) <= 0) {
			SMTP_auth_error(sock, GT_("Bad base64 reply from server.\n"));
			return;
		}
		if (outlevel >= O_DEBUG)
			report(stdout, GT_("Challenge decoded: %s\n"), b64buf);
		hmac_md5(password, strlen(password),
			 b64buf, strlen(b64buf), digest, sizeof(digest));
#ifdef HAVE_SNPRINTF
		snprintf(tmp, sizeof(tmp),
#else
		sprintf(tmp,
#endif /* HAVE_SNPRINTF */
		"%s %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		username,  digest[0], digest[1], digest[2], digest[3],
		digest[4], digest[5], digest[6], digest[7], digest[8],
		digest[9], digest[10], digest[11], digest[12], digest[13],
		digest[14], digest[15]);

		to64frombits(b64buf, tmp, strlen(tmp));
		SockPrintf(sock, "%s\r\n", b64buf);
		SMTP_ok(sock);
	}
	else if (strstr(buf, "PLAIN")) {
		int len;
		if (outlevel >= O_MONITOR)
			report(stdout, GT_("ESMTP PLAIN Authentication...\n"));
#ifdef HAVE_SNPRINTF
		snprintf(tmp, sizeof(tmp),
#else
		sprintf(tmp,
#endif /* HAVE_SNPRINTF */
		"^%s^%s", username, password);

		len = strlen(tmp);
		for (c = len - 1; c >= 0; c--)
		{
			if (tmp[c] == '^')
				tmp[c] = '\0';
		}
		to64frombits(b64buf, tmp, len);
		SockPrintf(sock, "AUTH PLAIN %s\r\n", b64buf);
		SMTP_ok(sock);
	}
	else if (strstr(buf, "LOGIN")) {
		if (outlevel >= O_MONITOR)
			report(stdout, GT_("ESMTP LOGIN Authentication...\n"));
		SockPrintf(sock, "AUTH LOGIN\r\n");
		SockRead(sock, smtp_response, sizeof(smtp_response) - 1);
		strncpy(tmp, smtp_response, sizeof(tmp));
		tmp[sizeof(tmp)-1] = '\0';

		if (strncmp(tmp, "334 ", 4)) { /* Server rejects AUTH */
			SMTP_auth_error(sock, GT_("Server rejected the AUTH command.\n"));
			return;
		}

		p = strchr(tmp, ' ');
		p++;
		if (from64tobits(b64buf, p, sizeof(b64buf) - 1) <= 0) {
			SMTP_auth_error(sock, GT_("Bad base64 reply from server.\n"));
			return;
		}
		to64frombits(b64buf, username, strlen(username));
		SockPrintf(sock, "%s\r\n", b64buf);
		SockRead(sock, smtp_response, sizeof(smtp_response) - 1);
		strncpy(tmp, smtp_response, sizeof(tmp));
		tmp[sizeof(tmp)-1] = '\0';
		p = strchr(tmp, ' ');
		if (!p) {
			SMTP_auth_error(sock, GT_("Bad base64 reply from server.\n"));
			return;
		}
		p++;
		memset(b64buf, 0, sizeof(b64buf));
		if (from64tobits(b64buf, p, sizeof(b64buf) - 1) <= 0) {
			SMTP_auth_error(sock, GT_("Bad base64 reply from server.\n"));
			return;
		}
		to64frombits(b64buf, password, strlen(password));
		SockPrintf(sock, "%s\r\n", b64buf);
		SMTP_ok(sock);
	}
	return;
}

int SMTP_ehlo(int sock, const char *host, char *name, char *password, int *opt)
/* send a "EHLO" message to the SMTP listener, return extension status bits */
{
  struct opt *hp;
  char auth_response[511];

  SockPrintf(sock,"%cHLO %s\r\n", (smtp_mode == 'S') ? 'E' : smtp_mode, host);
  if (outlevel >= O_MONITOR)
      report(stdout, "%cMTP> %cHLO %s\n", 
	    smtp_mode, (smtp_mode == 'S') ? 'E' : smtp_mode, host);
  
  *opt = 0;
  while ((SockRead(sock, smtp_response, sizeof(smtp_response)-1)) != -1)
  {
      int  n = strlen(smtp_response);

      if (smtp_response[strlen(smtp_response)-1] == '\n')
	  smtp_response[strlen(smtp_response)-1] = '\0';
      if (smtp_response[strlen(smtp_response)-1] == '\r')
	  smtp_response[strlen(smtp_response)-1] = '\0';
      if (n < 4)
	  return SM_ERROR;
      smtp_response[n] = '\0';
      if (outlevel >= O_MONITOR)
	  report(stdout, "SMTP< %s\n", smtp_response);
      for (hp = extensions; hp->name; hp++)
	  if (!strncasecmp(hp->name, smtp_response+4, strlen(hp->name))) {
	      *opt |= hp->value;
	      if (strncmp(hp->name, "AUTH ", 5) == 0)
	      	strncpy(auth_response, smtp_response, sizeof(auth_response));
		auth_response[sizeof(auth_response)-1] = '\0';
	  }
      if ((smtp_response[0] == '1' || smtp_response[0] == '2' || smtp_response[0] == '3') && smtp_response[3] == ' ') {
	  if (*opt & ESMTP_AUTH)
		SMTP_auth(sock, name, password, auth_response);
	  return SM_OK;
      }
      else if (smtp_response[3] != '-')
	  return SM_ERROR;
  }
  return SM_UNRECOVERABLE;
}

int SMTP_from(int sock, const char *from, const char *opts)
/* send a "MAIL FROM:" message to the SMTP listener */
{
    int ok;
    char buf[MSGBUFSIZE];

    if (strchr(from, '<'))
#ifdef HAVE_SNPRINTF
	snprintf(buf, sizeof(buf),
#else
	sprintf(buf,
#endif /* HAVE_SNPRINTF */
		"MAIL FROM: %s", from);
    else
#ifdef HAVE_SNPRINTF
    snprintf(buf, sizeof(buf),
#else
    sprintf(buf,
#endif /* HAVE_SNPRINTF */
	    "MAIL FROM:<%s>", from);
    if (opts)
#ifdef HAVE_SNPRINTF
	snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%s", opts);
#else
	strcat(buf, opts);
#endif /* HAVE_SNPRINTF */
    SockPrintf(sock,"%s\r\n", buf);
    if (outlevel >= O_MONITOR)
	report(stdout, "%cMTP> %s\n", smtp_mode, buf);
    ok = SMTP_ok(sock);
    return ok;
}

int SMTP_rcpt(int sock, const char *to)
/* send a "RCPT TO:" message to the SMTP listener */
{
  int ok;

  SockPrintf(sock,"RCPT TO:<%s>\r\n", to);
  if (outlevel >= O_MONITOR)
      report(stdout, "%cMTP> RCPT TO:<%s>\n", smtp_mode, to);
  ok = SMTP_ok(sock);
  return ok;
}

int SMTP_data(int sock)
/* send a "DATA" message to the SMTP listener */
{
  int ok;

  SockPrintf(sock,"DATA\r\n");
  if (outlevel >= O_MONITOR)
      report(stdout, "%cMTP> DATA\n", smtp_mode);
  ok = SMTP_ok(sock);
  return ok;
}

int SMTP_rset(int sock)
/* send a "RSET" message to the SMTP listener */
{
  int ok;

  SockPrintf(sock,"RSET\r\n");
  if (outlevel >= O_MONITOR)
      report(stdout, "%cMTP> RSET\n", smtp_mode);
  ok = SMTP_ok(sock);
  return ok;
}

int SMTP_quit(int sock)
/* send a "QUIT" message to the SMTP listener */
{
  int ok;

  SockPrintf(sock,"QUIT\r\n");
  if (outlevel >= O_MONITOR)
      report(stdout, "%cMTP> QUIT\n", smtp_mode);
  ok = SMTP_ok(sock);
  return ok;
}

int SMTP_eom(int sock)
/* send a message data terminator to the SMTP listener */
{
  int ok;

  SockPrintf(sock,".\r\n");
  if (outlevel >= O_MONITOR)
      report(stdout, "%cMTP>. (EOM)\n", smtp_mode);

  /* 
   * When doing LMTP, must process many of these at the outer level. 
   */
  if (smtp_mode == 'S')
      ok = SMTP_ok(sock);
  else
      ok = SM_OK;

  return ok;
}

time_t last_smtp_ok = 0;

int SMTP_ok(int sock)
/* returns status of SMTP connection */
{
    SIGHANDLERTYPE alrmsave;

    /* set an alarm for smtp ok */
    alrmsave = set_signal_handler(SIGALRM, null_signal_handler);
    set_timeout(mytimeout);

    while ((SockRead(sock, smtp_response, sizeof(smtp_response)-1)) != -1)
    {
	int n;

	/* restore alarm */
	set_timeout(0);
	set_signal_handler(SIGALRM, alrmsave);

	n = strlen(smtp_response);
	if (n > 0 && smtp_response[n-1] == '\n')
	    n--;
	if (n > 0 && smtp_response[n-1] == '\r')
	    n--;
	smtp_response[n] = '\0';
	if (outlevel >= O_MONITOR)
	    report(stdout, "%cMTP< %s\n", smtp_mode, smtp_response);
	if (n < 4 ||
	    (smtp_response[3] != ' ' && smtp_response[3] != '-'))
	{
	    if (outlevel >= O_MONITOR)
		report(stderr, GT_("smtp listener protocol error\n"));
	    return SM_UNRECOVERABLE;
	}

	last_smtp_ok = time((time_t *) NULL);

	if ((smtp_response[0] == '1' || smtp_response[0] == '2' || smtp_response[0] == '3') &&
	    smtp_response[3] == ' ')
	    return SM_OK;
	else if (smtp_response[3] != '-')
	    return SM_ERROR;

	/* set an alarm for smtp ok */
	set_signal_handler(SIGALRM, null_signal_handler);
	set_timeout(mytimeout);

    }

    /* restore alarm */
    set_timeout(0);
    set_signal_handler(SIGALRM, alrmsave);

    if (outlevel >= O_MONITOR)
	report(stderr, GT_("smtp listener protocol error\n"));
    return SM_UNRECOVERABLE;
}

/* smtp.c ends here */
