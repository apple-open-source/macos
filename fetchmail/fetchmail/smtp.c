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
#include "fetchmail.h"
#include "socket.h"
#include "smtp.h"
#include "config.h"

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

int SMTP_ehlo(int sock, const char *host, int *opt)
/* send a "EHLO" message to the SMTP listener, return extension status bits */
{
  struct opt *hp;

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
	  if (!strncasecmp(hp->name, smtp_response+4, strlen(hp->name)))
	      *opt |= hp->value;
      if ((smtp_response[0] == '1' || smtp_response[0] == '2' || smtp_response[0] == '3') && smtp_response[3] == ' ')
	  return SM_OK;
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
	sprintf(buf, "MAIL FROM: %s", from);
    else
	sprintf(buf, "MAIL FROM:<%s>", from);
    if (opts)
	strcat(buf, opts);
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

int SMTP_ok(int sock)
/* returns status of SMTP connection */
{
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
	    report(stdout, "%cMTP< %s\n", smtp_mode, smtp_response);
	if ((smtp_response[0] == '1' || smtp_response[0] == '2' || smtp_response[0] == '3') && smtp_response[3] == ' ')
	    return SM_OK;
	else if (smtp_response[3] != '-')
	    return SM_ERROR;
    }
    return SM_UNRECOVERABLE;
}

/* smtp.c ends here */
