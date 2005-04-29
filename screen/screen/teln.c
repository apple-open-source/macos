/* Copyright (c) 1993-2002
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 ****************************************************************
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>

#include "config.h"

#ifdef BUILTIN_TELNET

#include "screen.h"
#include "extern.h"

extern struct win *fore;
extern struct layer *flayer;
extern int visual_bell;
extern char screenterm[];

static void TelReply __P((struct win *, char *, int));
static void TelDocmd __P((struct win *, int, int));
static void TelDosub __P((struct win *));

#define TEL_DEFPORT	23
#define TEL_CONNECTING	(-2)

#define TC_IAC          255
#define TC_DONT         254
#define TC_DO           253
#define TC_WONT         252
#define TC_WILL         251
#define TC_SB           250
#define TC_BREAK        243
#define TC_SE           240

#define TC_S "S  b      swWdDc"

#define TO_BINARY       0
#define TO_ECHO         1
#define TO_SGA          3
#define TO_TM           6
#define TO_TTYPE        24
#define TO_NAWS         31
#define TO_TSPEED       32
#define TO_LFLOW        33
#define TO_XDISPLOC     35
#define TO_NEWENV       39

#define TO_S "be c                    t      wsf xE  E"


static unsigned char tn_init[] = {
  TC_IAC, TC_DO, TO_SGA,
  TC_IAC, TC_WILL, TO_TTYPE,
  TC_IAC, TC_WILL, TO_NAWS,
  TC_IAC, TC_WILL, TO_LFLOW,
};

static void
tel_connev_fn(ev, data)
struct event *ev;
char *data;
{
  struct win *p = (struct win *)data;
  if (connect(p->w_ptyfd, (struct sockaddr *)&p->w_telsa, sizeof(p->w_telsa)) && errno != EISCONN)
    {
      char buf[1024];
      buf[0] = ' ';
      strncpy(buf + 1, strerror(errno), sizeof(buf) - 2);
      buf[sizeof(buf) - 1] = 0;
      WriteString(p, buf, strlen(buf));
      WindowDied(p);
      return;
    }
  WriteString(p, "connected.\r\n", 12);
  evdeq(&p->w_telconnev);
  p->w_telstate = 0;
}

int
TelOpen(args)
char **args;
{
  int fd;
  int on = 1;

  if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    {
      Msg(errno, "TelOpen: socket");
      return -1;
    }
  if (setsockopt(fd, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on)))
    Msg(errno, "TelOpen: setsockopt SO_OOBINLINE");
  return fd;
}

int
TelConnect(p)
struct win *p;
{
  int port = TEL_DEFPORT;
  struct hostent *hp;
  char **args;
  char buf[256];

  args = p->w_cmdargs + 1;

  if (!*args)
    {
      Msg(0, "Usage: screen //telnet host [port]");
      return -1;
    }
  if (args[1])
    port = atoi(args[1]);
  p->w_telsa.sin_family = AF_INET;
  if((p->w_telsa.sin_addr.s_addr = inet_addr(*args)) == -1)
    {
      if ((hp = gethostbyname(*args)) == NULL)
        {
	  Msg(0, "unknown host: %s", *args);
	  return -1;
        }
      if (hp->h_length != sizeof(p->w_telsa.sin_addr.s_addr) || hp->h_addrtype != AF_INET)
	{
	  Msg(0, "Bad address type for %s", hp->h_name);
	  return -1;
	}
      bcopy((char *)hp->h_addr,(char *)&p->w_telsa.sin_addr.s_addr, hp->h_length);
      p->w_telsa.sin_family = hp->h_addrtype;
    }
  p->w_telsa.sin_port = htons(port);
  if (port != TEL_DEFPORT)
    sprintf(buf, "Trying %s %d...", inet_ntoa(p->w_telsa.sin_addr), port);
  else
    sprintf(buf, "Trying %s...", inet_ntoa(p->w_telsa.sin_addr));
  WriteString(p, buf, strlen(buf));
  if (connect(p->w_ptyfd, (struct sockaddr *)&p->w_telsa, sizeof(p->w_telsa)))
    {
      if (errno == EINPROGRESS)
        {
	  p->w_telstate = TEL_CONNECTING;
	  p->w_telconnev.fd = p->w_ptyfd;
	  p->w_telconnev.handler = tel_connev_fn;
	  p->w_telconnev.data = (char *)p;
	  p->w_telconnev.type = EV_WRITE;
	  p->w_telconnev.pri = 1;
	  debug("telnet connect in progress...\n");
	  evenq(&p->w_telconnev);
	}
      else
        {
	  Msg(errno, "TelOpen: connect");
	  return -1;
	}
    }
  else
    WriteString(p, "connected.\r\n", 12);
  if (port == TEL_DEFPORT)
    TelReply(p, (char *)tn_init, sizeof(tn_init));
  return 0;
}

int
TelIsline(p)
struct win *p;
{
  return !fore->w_telropts[TO_SGA];
}

void
TelProcessLine(bufpp, lenp)
char **bufpp;
int *lenp;
{
  int echo = !fore->w_telropts[TO_ECHO];
  unsigned char c;
  char *tb;
  int tl;

  char *buf = *bufpp;
  int l = *lenp;
  while (l--)
    {
      c = *(unsigned char *)buf++;
      if (fore->w_telbufl + 2 >= IOSIZE)
	{
	  WBell(fore, visual_bell);
	  continue;
	}
      if (c == '\r')
	{
	  if (echo)
	    WriteString(fore, "\r\n", 2);
	  fore->w_telbuf[fore->w_telbufl++] = '\r';
	  fore->w_telbuf[fore->w_telbufl++] = '\n';
	  tb = fore->w_telbuf;
	  tl = fore->w_telbufl;
	  LayProcess(&tb, &tl);
	  fore->w_telbufl = 0;
	  continue;
	}
      if (c == '\b' && fore->w_telbufl > 0)
	{
	  if (echo)
	    {
	      WriteString(fore, (char *)&c, 1);
	      WriteString(fore, " ", 1);
	      WriteString(fore, (char *)&c, 1);
	    }
	  fore->w_telbufl--;
	}
      if ((c >= 0x20 && c <= 0x7e) || c >= 0xa0)
	{
	  if (echo)
	    WriteString(fore, (char *)&c, 1);
	  fore->w_telbuf[fore->w_telbufl++] = c;
	}
    }
  *lenp = 0;
}

int
DoTelnet(buf, lenp, f)
char *buf;
int *lenp;
int f;
{
  int echo = !fore->w_telropts[TO_ECHO];
  int cmode = fore->w_telropts[TO_SGA];
  int bin = fore->w_telropts[TO_BINARY];
  char *p = buf, *sbuf;
  int trunc = 0;
  int c;
  int l = *lenp;

  sbuf = p;
  while (l-- > 0)
    {
      c = *(unsigned char *)p++;
      if (c == TC_IAC || (c == '\r' && (l ==0 || *p != '\n') && cmode && !bin))
	{
	  if (cmode && echo)
	    {
	      WriteString(fore, sbuf, p - sbuf);
	      sbuf = p;
	    }
	  if (f-- <= 0)
	    {
	      trunc++;
	      l--;
	    }
	  if (l < 0)
	    {
	      p--;	/* drop char */
	      break;
	    }
	  if (l)
	    bcopy(p, p + 1, l);
	  if (c == TC_IAC)
	    *p++ = c;
	  else if (c == '\r')
	    *p++ = 0;
	  else if (c == '\n')
	    {
	      p[-1] = '\r';
	      *p++ = '\n';
	    }
	}
    }
  *lenp = p - buf;
  return trunc;
}

/* modifies data in-place, returns new length */
int
TelIn(p, buf, len, free)
struct win *p;
char *buf;
int len;
int free;
{
  char *rp, *wp;
  int c;

  rp = wp = buf;
  while (len-- > 0)
    {
      c = *(unsigned char *)rp++;

      if (p->w_telstate >= TC_WILL && p->w_telstate <= TC_DONT)
	{
	  TelDocmd(p, p->w_telstate, c);
	  p->w_telstate = 0;
	  continue;
	}
      if (p->w_telstate == TC_SB || p->w_telstate == TC_SE)
	{
	  if (p->w_telstate == TC_SE && c == TC_IAC)
	    p->w_telsubidx--;
	  if (p->w_telstate == TC_SE && c == TC_SE)
	    {
	      p->w_telsubidx--;
	      TelDosub(p);
	      p->w_telstate = 0;
	      continue;
	    }
	  if (p->w_telstate == TC_SB && c == TC_IAC)
	    p->w_telstate = TC_SE;
	  else
	    p->w_telstate = TC_SB;
	  p->w_telsubbuf[p->w_telsubidx] = c;
	  if (p->w_telsubidx < sizeof(p->w_telsubbuf) - 1)
	    p->w_telsubidx++;
	  continue;
	}
      if (p->w_telstate == TC_IAC)
	{
	  if ((c >= TC_WILL && c <= TC_DONT) || c == TC_SB)
	    {
	      p->w_telsubidx = 0;
	      p->w_telstate = c;
	      continue;
	    }
	  p->w_telstate = 0;
	  if (c != TC_IAC)
	    continue;
	}
      else if (c == TC_IAC)
	{
	  p->w_telstate = c;
	  continue;
	}
      if (p->w_telstate == '\r')
	{
	  p->w_telstate = 0;
	  if (c == 0)
	    continue;		/* suppress trailing \0 */
	}
      else if (c == '\n' && !p->w_telropts[TO_SGA])
	{
	  /* oops... simulate terminal line mode: insert \r */
	  if (wp + 1 == rp)
	    {
	      if (free-- > 0)
		{
		  if (len)
		    bcopy(rp, rp + 1, len);
		  rp++;
		  *wp++ = '\r';
		}
	    }
	  else
	    *wp++ = '\r';
	}
      if (c == '\r')
        p->w_telstate = c;
      *wp++ = c;
    }
  return wp - buf;
}

static void
TelReply(p, str, len)
struct win *p;
char *str;
int len;
{
  if (len <= 0)
    return;
  if (p->w_inlen + len > IOSIZE)
    {
      Msg(0, "Warning: telnet protocol overrun!");
      return;
    }
  bcopy(str, p->w_inbuf + p->w_inlen, len);
  p->w_inlen += len;
}

static void
TelDocmd(p, cmd, opt)
struct win *p;
int cmd, opt;
{
  unsigned char b[3];
  int repl = 0;

  if (cmd == TC_WONT)
    debug2("[WONT %c %d]\n", TO_S[opt], opt);
  if (cmd == TC_WILL)
    debug2("[WILL %c %d]\n", TO_S[opt], opt);
  if (cmd == TC_DONT)
    debug2("[DONT %c %d]\n", TO_S[opt], opt);
  if (cmd == TC_DO)
    debug2("[DO  %c %d]\n", TO_S[opt], opt);

  switch(cmd)
    {
    case TC_WILL:
      if (p->w_telropts[opt] || opt == TO_TM)
	return;
      repl = TC_DONT;
      if (opt == TO_ECHO || opt == TO_SGA || opt == TO_BINARY)
	{
	  p->w_telropts[opt] = 1;
	  /* setcon(); */
	  repl = TC_DO;
	}
      break;
    case TC_WONT:
      if (!p->w_telropts[opt] || opt == TO_TM)
	return;
      repl = TC_DONT;
#if 0
      if (opt == TO_ECHO || opt == TO_SGA)
	setcon();
#endif
      p->w_telropts[opt] = 0;
      break;
    case TC_DO:
      if (p->w_telmopts[opt])
	return;
      repl = TC_WONT;
      if (opt == TO_TTYPE || opt == TO_SGA || opt == TO_BINARY || opt == TO_NAWS || opt == TO_TM || opt == TO_LFLOW)
	{
	  repl = TC_WILL;
	  p->w_telmopts[opt] = 1;
	}
      p->w_telmopts[TO_TM] = 0;
      break;
    case TC_DONT:
      if (!p->w_telmopts[opt])
	return;
      repl = TC_WONT;
      p->w_telmopts[opt] = 0;
      break;
    }
  b[0] = TC_IAC;
  b[1] = repl;
  b[2] = opt;
  TelReply(p, (char *)b, 3);
  if (cmd == TC_DO && opt == TO_NAWS)
    TelWindowSize(p);
}


static void
TelDosub(p)
struct win *p;
{
  char trepl[20 + 6 + 1];
  int l;

  switch(p->w_telsubbuf[0])
    {
    case TO_TTYPE:
      if (p->w_telsubidx != 2 || p->w_telsubbuf[1] != 1)
	return;
      l = strlen(screenterm);
      if (l >= 20)
	break;
      sprintf(trepl, "%c%c%c%c%s%c%c", TC_IAC, TC_SB, TO_TTYPE, 0, screenterm, TC_IAC, TC_SE);
      TelReply(p, trepl, l + 6);
      break;
    case TO_LFLOW:
      if (p->w_telsubidx != 2)
	return;
      debug1("[FLOW %d]\r\n", p->w_telsubbuf[1]);
      break;
    default:
      break;
   }
}

void
TelBreak(p)
struct win *p;
{
  static unsigned char tel_break[] = { TC_IAC, TC_BREAK };
  TelReply(p, (char *)tel_break, 2);
}

void
TelWindowSize(p)
struct win *p;
{
  char s[20], trepl[20], *t;
  int i;

  debug2("TelWindowSize %d %d\n", p->w_width, p->w_height);
  if (p->w_width == 0 || p->w_height == 0 || !p->w_telmopts[TO_NAWS])
    return;
  sprintf(s, "%c%c%c%c%c%c%c%c%c", TC_SB, TC_SB, TO_NAWS, p->w_width / 256, p->w_width & 255, p->w_height / 256, p->w_height & 255, TC_SE, TC_SE);
  t = trepl;
  for (i = 0; i < 9; i++)
    if ((unsigned char)(*t++ = s[i]) == TC_IAC)
      *t++ = TC_IAC;
  trepl[0] = TC_IAC;
  t[-2] = TC_IAC;
  debug(" - sending");
  for (i = 0; trepl + i < t; i++)
    debug1(" %02x", (unsigned char)trepl[i]);
  debug("\n");
  TelReply(p, trepl, t - trepl);
}

static char tc_s[] = TC_S;
static char to_s[] = TO_S;

void
TelStatus(p, buf, l)
struct win *p;
char *buf;
int l;
{
  int i;

  *buf++ = '[';
  for (i = 0; to_s[i]; i++)
    {
      if (to_s[i] == ' ' || p->w_telmopts[i] == 0)
	continue;
      *buf++ = to_s[i];
    }
  *buf++ = ':';
  for (i = 0; to_s[i]; i++)
    {
      if (to_s[i] == ' ' || p->w_telropts[i] == 0)
	continue;
      *buf++ = to_s[i];
    }
  if (p->w_telstate == TEL_CONNECTING)
    buf[-1] = 'C';
  else if (p->w_telstate && p->w_telstate != '\r')
    {
      *buf++ = ':';
      *buf++ = tc_s[p->w_telstate - TC_SE];
    }
  *buf++ = ']';
  *buf = 0;
  return;
}

#endif /* BUILTIN_TELNET */
