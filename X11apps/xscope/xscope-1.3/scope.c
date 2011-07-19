/* ******************************************************
 *						      	*
 *  A spy program to reveal X11  traffic	    	*
 *						      	*
 *	James Peterson, 1988	       			*
 * Copyright (C) 1988 MCC
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of MCC not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  MCC makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * MCC DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL MCC BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *							*
 *							*
 * Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 ***************************************************** */

#include "scope.h"
#include "nas.h"
#include "extensions.h"

#include <ctype.h>
#include <unistd.h>

#include <sys/types.h>	       /* needed by sys/socket.h and netinet/in.h */
#include <sys/uio.h>	       /* for struct iovec, used by socket.h */
#include <sys/socket.h>	       /* for AF_INET, SOCK_STREAM, ... */
#include <sys/ioctl.h>	       /* for FIONCLEX, FIONBIO, ... */
#if !defined(FIOCLEX) && defined(HAVE_SYS_FILIO_H)
#include <sys/filio.h>
#endif
#include <fcntl.h>
#include <netinet/in.h>	       /* struct sockaddr_in */
#include <netinet/tcp.h>
#include <netdb.h>	       /* struct servent * and struct hostent * */
#include <errno.h>	       /* for EINTR, EADDRINUSE, ... */


/* ********************************************** */
/*                                                */
/* ********************************************** */

#define DefaultPort 6000

       char    ServerHostName[MAXHOSTNAMELEN];
       char    AudioServerHostName[MAXHOSTNAMELEN];
       long    ServerBasePort = DefaultPort;
static long    ServerInPort = 1;
static long    ServerOutPort = 0;
static long    ServerDisplay = 0;

static FD ConnectToClient(FD ConnectionSocket);
static void DataFromClient(FD fd);
static void SetUpStdin(void);

long	TranslateText = 0;
char    ScopeEnabled = 1;
char    HandleSIGUSR1 = 0;
char	DoAudio = 0;
char	TerminateClose = 0;
int	Interrupt = 0;

struct FDDescriptor *FDD = NULL;
short MaxFD = 0;
short nFDsInUse = 0;
fd_set ReadDescriptors;
fd_set WriteDescriptors;
fd_set BlockedReadDescriptors;
short HighestFD;

short	debuglevel = 0;
short	Verbose = 0;
short	XVerbose = 0;
short	NasVerbose = 0;
short	Raw = 0;
int	ScopePort = 0;
char	*ScopeHost = NULL;


typedef struct _BreakPoint {
  struct _BreakPoint	*next;
  int			number;
  unsigned char		request;
  int			minorop;
  Boolean		enabled;  
} BP;

typedef enum _CMDResult { CMDProceed, CMDDebug, CMDSyntax, CMDError } CMDResult;

static CMDResult   CMDStep	(int argc, char **argv);
static CMDResult   CMDCont	(int argc, char **argv);
static CMDResult   CMDBreak	(int argc, char **argv);
static CMDResult   CMDDelete	(int argc, char **argv);
static CMDResult   CMDDisable	(int argc, char **argv);
static CMDResult   CMDEnable	(int argc, char **argv);
static CMDResult   CMDLevel	(int argc, char **argv);
static CMDResult   CMDAudio	(int argc, char **argv);
static CMDResult   CMDQuit	(int argc, char **argv);
static CMDResult   CMDHelp	(int argc, char **argv);

typedef struct _CMDFunc {
    char	*name;
    char	*alias;
    CMDResult	(*func)(int argc, char **argv);
    char	*usage;
    char	*help;
} CMDFuncRec;
typedef const CMDFuncRec *CMDFuncPtr;

static const CMDFuncRec  CMDFuncs[] = {
  { "audio",  "a",  CMDAudio,	"[a]udio",
    "Set audio output level\n" },
  { "break",  "b",  CMDBreak,	"[b]reak",
    "Create breakpoints\n" },
  { "cont",   "c",  CMDCont,	"[c]ont",
    "Continue scoping\n" },
  { "delete", "del", CMDDelete,	"[del]ete",
    "Delete breakpoints\n" },
  { "disable","d",  CMDDisable,	"[d]isable",
    "Disable breakpoints\n" },
  { "enable", "e",  CMDEnable,	"[e]nable",
    "Enable breakpoints\n" },
  { "help",   "?",  CMDHelp,	"help",
    "Print this list\n" },
  { "level",  "l",  CMDLevel,	"[l]evel",
    "Set output level\n" },
  { "quit",   "q",  CMDQuit,	"[q]uit",
    "Quit Xscope\n" },
  { "step",   "s",  CMDStep,	"[s]tep",
    "Step trace one request\n" },
};

#define NumCMDFuncs (sizeof CMDFuncs / sizeof CMDFuncs[0])

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

static int
CMDStringToInt (
    char *s,
    int *v)
{
    int	sign = 1;

    switch (*s) {
    case '-':
	sign = -1;
	s++;
	break;
    case '+':
	s++;
	break;
    }

    if (!strncmp (s, "0x", 2))
    {
	sscanf (s + 2, "%x", v);
    } 
    else if (*s == '0')
    {
	sscanf (s, "%o", v);
    }
    else if (isdigit (*s))
    {
	sscanf (s, "%d", v);
    }
    else if (*s == '\'')
    {
	s++;
	if (*s == '\\') {
	    s++;
	    switch (*s) {
	    case 'n':
		*v = '\n';
		break;
	    case 't':
		*v = '\t';
		break;
	    default:
		*v = (int) *s;
		break;
	    }
	} else
	    *v = (int) *s;
	s++;
	if (*s != '\'')
	    return FALSE;
    }
    else
	return FALSE;
    *v *= sign;
    return TRUE;
}

static CMDFuncPtr
CMDStringToFunc (
    const char *name)
{
    int	    i;
    for (i = 0; i < NumCMDFuncs; i++)
    {
	if (!strcmp (name, CMDFuncs[i].name) ||
	    !strcmp (name, CMDFuncs[i].alias))
	{
	    return &CMDFuncs[i];
	}
    }
    return NULL;
}    

static int
CMDSplitIntoWords (
    char *line,
    char **argv)
{
    char    quotechar;
    int	    argc;
    
    argc = 0;
    while (*line) {
	while (isspace(*line))
	    line++;
	if (!*line)
	    break;
	if (*line == '"') 
	{
	    quotechar = *line++;
	    *argv++ = line;
	    argc++;
	    while (*line && *line != quotechar)
		line++;
	    if (*line)
		*line++ = '\0';
	}
	else 
	{
	    *argv++ = line;
	    argc++;
	    while (*line && !isspace(*line))
		line++;
	    if (*line)
		*line++ = '\0';
	}
    }
    *argv = NULL;
    return argc;
}

static CMDResult
CMDHelp(
    int argc,
    char **argv)
{
    int		i;
    CMDFuncPtr	func;
    
    if (argc == 1)
    {
	for (i = 0; i < NumCMDFuncs; i++)
	    printf("%-10s%s\n", CMDFuncs[i].name, CMDFuncs[i].usage);
    }
    else
    {
	for (i = 1; i < argc; i++)
	{
	    func = CMDStringToFunc (argv[i]);
	    if (!func)
	    {
		printf ("%-10s unknown command\n", argv[i]);
		return CMDSyntax;
	    }
	    printf ("%-10s %s\n%s", func->name, func->usage, func->help);
	}
    }
    return CMDDebug;
}
    
static void
CMDSyntaxError(
    int argc,
    char **argv)
{
    printf("Syntax error in:");
    while (*argv)
	printf(" %s", *argv++);
    printf("\n");
}

void
ReadCommands (void)
{
    int		argc;
    char 	line[1024];
    char    	*argv[20];
    CMDResult	result;
    CMDFuncPtr	func;
    static int	here;
    
    if (here)
	return;
    here = 1;
    for (;;) {
	printf ("> ");
	if (!fgets (line, sizeof line, stdin)) {
	    if(feof(stdin)) {
		strncpy(line, "quit", sizeof(line));
	    } else {
		printf("Error: %s\n", strerror(errno));
		break;
	    }
	}
	argc = CMDSplitIntoWords(line, argv);
	if (argc > 0) {
	    func = CMDStringToFunc (argv[0]);
	    if (!func)
		CMDSyntaxError (argc, argv);
	    else
	    {
		result = (*func->func)(argc, argv);
		switch (result) {
		case CMDSyntax:
		    CMDSyntaxError(argc, argv);
		    break;
		case CMDError:
		    printf ("Error\n");
		    break;
		case CMDProceed:
		    here = 0;
		    return;
		default:
		    break;
		}
	    }
	}
    }
    printf("...\n");
    here = 0;
}

int SingleStep;
int BreakPoint;
BP  *breakPoints;
int	    breakPointNumber;

void
TestBreakPoints (
    const unsigned char *buf,
    long n)
{
  BP  *bp;

  for (bp = breakPoints; bp; bp = bp->next)
  {
    if (bp->enabled)
    {
      if (bp->request == buf[0])
      {
	if (bp->request < EXTENSION_MIN_REQ) /* Core protocol, not extension */
	  break;
	else if ((bp->minorop == -1) || (bp->minorop == buf[1]))
	  /* extension, either matching minor opcode or all minor opcodes */
	  break;
      }
    }
  }
  if (bp)
  {
    if (bp->request < EXTENSION_MIN_REQ)
      printf ("Breakpoint on request %d\n", bp->request);
    else if (bp->minorop == -1)
      printf ("Breakpoint on extension %d\n", bp->request);
    else
      printf ("Breakpoint on extension %d, minor request %d\n",
	      bp->request, bp->minorop);
    ReadCommands ();
  }
}

static void
setBreakPoint (void)
{
  Boolean b = false;
  BP  *bp;
  FD  fd;

  if (SingleStep)
    b = true;
  else
  {
    for (bp = breakPoints; bp; bp = bp->next)
    {
      if (bp->enabled)
      {
	b = true;
	break;
      }
    }
  }
  if (b != BreakPoint)
  {
    BreakPoint = b;
    for (fd = 0; fd < HighestFD; fd++)
    {
      if (FDD[fd].Busy && FDD[fd].InputHandler == DataFromClient)
      {
	if (BreakPoint)
	  SetBufLimit (fd);
	else
	  ClearBufLimit (fd);
      }
    }
  }
}

static CMDResult
CMDBreak (
    int argc,
    char **argv)
{
  BP  *bp, **prev;
  int   request, minorop;
  char *minorname;
  
  if (argc == 1)
  {
    for (bp = breakPoints; bp; bp = bp->next)
    {
      printf ("%3d: %s %3d ", bp->number,
	      bp->enabled ? "enabled" : "disabled",
	      bp->request);
      PrintENUMERATED(&bp->request, 1, TD[REQUEST].ValueList);
      if (bp->minorop != -1)
	printf (":%d", bp->minorop);
      printf ("\n");
    }
  }
  else
  {
    for (prev = &breakPoints; *prev; prev = &(*prev)->next);
    while (*++argv) {
      if ((minorname = strchr(*argv, ':'))) {
	*minorname = '\0';
	if (!CMDStringToInt (minorname + 1, &minorop) ||
	    (minorop < 0) || (minorop > 255)) {
	  *minorname = ':'; /* restore string for error message */
	  return CMDSyntax;
	}
      } else {
	minorop = -1;
      }
      request = GetXRequestFromName (*argv);
      if (request < 0 && !CMDStringToInt (*argv, &request))
	return CMDSyntax;
      if ((request < EXTENSION_MIN_REQ) && (minorname != NULL)) {
	*minorname = ':'; /* restore string for error message */
	return CMDSyntax;
      }
      bp = (BP *) malloc (sizeof (BP));
      bp->number = ++breakPointNumber;
      bp->request = request;
      bp->minorop = minorop;
      bp->enabled = true;
      bp->next = NULL;
      *prev = bp;
      prev = &bp->next;
    }
  }
  setBreakPoint ();
  return CMDDebug;
}

static CMDResult
CMDCont (
    int argc,
    char **argv)
{
    SingleStep = 0;
    return CMDProceed;
}

static CMDResult
CMDDisable (
    int argc,
    char **argv)
{
  BP  *bp;
  int number;
  
  if (argc == 1)
  {
    printf ("Disabling all breakpoints...\n");
    for (bp = breakPoints; bp; bp = bp->next)
      bp->enabled = false;
  }
  else
  {
    while (*++argv) {
      if (!CMDStringToInt (*argv, &number))
	return CMDSyntax;
      for (bp = breakPoints; bp; bp = bp->next)
	if (bp->number == number)
	{
	  bp->enabled = false;
	  break;
	}
      if (!bp)
      {
	printf ("No such breakpoint %s\n", *argv);
      }
    }
  }
  setBreakPoint ();
  return CMDDebug;
}

static CMDResult
CMDDelete (
    int argc,
    char **argv)
{
  BP  *bp, **prev;
  int	      number;
  
  if (argc == 1)
  {
    printf ("Deleting all breakpoints...\n");
    while ((bp = breakPoints) != NULL)
    {
      breakPoints = bp->next;
      free (bp);
    }
  }
  else
  {
    while (*++argv) {
      if (!CMDStringToInt (*argv, &number))
	return CMDSyntax;
      for (prev = &breakPoints; (bp = *prev) != NULL; prev = &bp->next)
      {
	if (bp->number == number)
	{
	  *prev = bp->next;
	  free (bp);
	  break;
	}
      }
      if (!bp)
      {
	printf ("No such breakpoint %s\n", *argv);
      }
    }
  }
  setBreakPoint ();
  return CMDDebug;
}

static CMDResult
CMDEnable (
    int argc,
    char **argv)
{
  BP  *bp;
  int	      number;
  
  if (argc == 1)
  {
    printf ("Enabling all breakpoints...\n");
    for (bp = breakPoints; bp; bp = bp->next)
      bp->enabled = true;
  }
  else
  {
    while (*++argv) {
      if (!CMDStringToInt (*argv, &number))
	return CMDSyntax;
      for (bp = breakPoints; bp; bp = bp->next)
	if (bp->number == number)
	{
	  bp->enabled = true;
	  break;
	}
      if (!bp)
      {
	printf ("No such breakpoint %s\n", *argv);
      }
    }
  }
  setBreakPoint ();
  return CMDDebug;
}

static CMDResult
CMDStep (
    int argc,
    char **argv)
{
    SingleStep = 1;
    setBreakPoint ();
    return CMDProceed;
}

static CMDResult
CMDQuit (
    int argc,
    char **argv)
{
  printf ("exiting...\n");
  exit (0);
}

static CMDResult
CMDLevel (
    int argc,
    char **argv)
{
  int	level;

  if (argc == 1)
    printf ("Level: %d\n", XVerbose);
  else if (argc == 2 && CMDStringToInt (argv[1], &level))
    XVerbose = level;
  else
    return CMDSyntax;
  return CMDDebug;
}

static CMDResult
CMDAudio (
    int argc,
    char **argv)
{
  int	level;

  if (argc == 1)
    printf ("Audio Level: %d\n", NasVerbose);
  else if (argc == 2 && CMDStringToInt (argv[1], &level))
    NasVerbose = level;
  else
    return CMDSyntax;
  return CMDDebug;
}

/* ********************************************** */
/*                                                */
/*                                                */
/* ********************************************** */

short
GetServerport (void)
{
  short     port;

  enterprocedure("GetServerport");

  port = ServerBasePort + ServerOutPort + ServerDisplay;
  debug(4,(stderr, "Server service is on port %d\n", port));
  return(port);
}

static short
GetScopePort (void)
{
  short     port;

  enterprocedure("GetScopePort");

  port = ServerBasePort + ServerInPort + ServerDisplay;
  debug(4,(stderr, "scope service is on port %d\n", port));
  return(port);
}

/* ********************************************** */
/*                                                */
/* ********************************************** */

static void 
Usage(void)
{
  fprintf(stderr, "Usage: xscope\n");
  fprintf(stderr, "              [-h<server-host>]\n");
  fprintf(stderr, "              [-i<in-port>]\n");
  fprintf(stderr, "              [-o<out-port>]\n");
  fprintf(stderr, "              [-d<display-number>]\n");
  fprintf(stderr, "              [-v<n>]  -- verbose output\n");
  fprintf(stderr, "              [-r]  -- raw output\n");
  fprintf(stderr, "              [-a<n>]  -- audio verbose output\n");
  fprintf(stderr, "              [-q]  -- quiet output\n");
  fprintf(stderr, "              [-D<debug-level>]\n");
  fprintf(stderr, "              [-S<n>] -- start/stop on SIGUSR1\n");
  fprintf(stderr, "              [-t]  -- terminate when all clients close\n");
  exit(1);
}


static void
ScanArgs (
    int     argc,
    char  **argv)
{
  XVerbose = 1 /* default verbose-ness level */;
  NasVerbose = 1;
  Raw = 0 ;

  /* Scan argument list */
  while (--argc > 0)
    {
      ++argv;
      if (**argv == '-')
	switch (*++*argv)
	  {
	    /*
	      debug levels:
	      	2 - trace each procedure entry
	      	4 - I/O, connections
	      	8 - Scope internals
	      	16 - Message protocol
		32 - 64 - malloc
		128 - 256 - really low level
	    */
	  case 'D':
	    debuglevel = atoi(++*argv);
	    if (debuglevel == 0)
	      debuglevel = 255;
	    XVerbose = 7;
	    debug(1,(stderr, "debuglevel = %d\n", debuglevel));
	    break;

	  case 'S':
	    HandleSIGUSR1 = 1;
	    ScopeEnabled = atoi(++*argv);
	    break;

	  case 'q': /* quiet mode */
	    XVerbose = 0;
	    debug(1,(stderr, "Verbose = %d\n", XVerbose));
	    break;

	  case 'r': /* raw mode */
	    Raw = 1 ;
	    debug(1,(stderr, "Raw = %d\n", Raw));
	    break;	    

	  case 'v': /* verbose mode */
	    XVerbose = atoi(++*argv);
	    debug(1,(stderr, "Verbose = %d\n", XVerbose));
	    break;

	  case 'o':
	    ServerOutPort = atoi(++*argv);
	    if (ServerOutPort <= 0)
	      ServerOutPort = 0;
	    debug(1,(stderr, "ServerOutPort = %ld\n", ServerOutPort));
	    break;

	  case 'd':
	    ServerDisplay = atoi(++*argv);
	    if (ServerDisplay <= 0)
	      ServerDisplay = 0;
	    debug(1,(stderr, "ServerDisplay = %ld\n", ServerDisplay));
	    break;

	  case 'i':
	    ServerInPort = atoi(++*argv);
	    if (ServerInPort <= 0)
	      ServerInPort = 0;
	    debug(1,(stderr, "ServerInPort = %ld\n", ServerInPort));
	    break;

	  case 'h':
	    if (++*argv != NULL && **argv != '\0' 
	      && (strlen(*argv) < sizeof(ServerHostName)))
	      strcpy(ServerHostName, *argv);
	    debug(1,(stderr, "ServerHostName=%s\n", ServerHostName));
	    break;

	  case 'T':
	    TranslateText = 1;
	    break;
	    
	  case 'A':
	    DoAudio = 1;
	    break;
	    
	  case 'a': /* verbose mode */
	    NasVerbose = atoi(++*argv);
	    debug(1,(stderr, "NasVerbose = %d\n", NasVerbose));
	    break;
	    
	  case 'n': /* NAS server host */
	    if (++*argv != NULL && **argv != '\0')
	      (void)strcpy(AudioServerHostName, *argv);
	    debug(1,(stderr, "AudioServerHostName=%s\n", AudioServerHostName));
	    break;
	  case 't':
	    TerminateClose = 1;
	    break;

	  default:
	    fprintf(stderr, "Unknown option %c\n", **argv);
	    Usage();
	    break;

	  }
      else
	{
	  /* file argument to scope -- error */
	  Usage();
	}
    }

  /* check for different port numbers or different machines */
  if (ServerInPort == ServerOutPort)
    if (ServerHostName[0] == '\0')
      {
	fprintf(stderr, "Can't have xscope on same port as server (%ld)\n",
		ServerInPort);
	Usage();
      }

}


/* ********************************************** */
/*                                                */
/* ********************************************** */

int
main (
    int     argc,
    char  **argv)
{
  ScanArgs(argc, argv);
  InitializeFD();
  InitializeX11();
  if (DoAudio)
    InitializeAudio();
  SetUpStdin();
  SetUpConnectionSocket(GetScopePort(), NewConnection);
  if (DoAudio)
    SetUpConnectionSocket (GetScopePort() + 2000, NewAudio);
  SetSignalHandling();

  return MainLoop();
}

void
TimerExpired (void)
{
  debug(16,(stderr, "Timer tick\n"));
}

/* ********************************************** */
/*                                                */
/* ********************************************** */

/*
  here is where we would add code to allow control from
  the keyboard.  We would want to read a command and
  interpret it.  Possibilties:

  (a) verbose level setting
  (b) reset time
  (c) save X requests to a file.
  (d) replay X requests from a file.
  (e) allow fake events, errors to be generated.
*/

static void
ReadStdin (
    FD fd)
{
  char    buf[2048];
  long    n;

  enterprocedure("ReadStdin");
  n = read(fd, buf, 2048);
  debug(4,(stderr, "read %ld bytes from stdin\n", n));
}

static void
SetUpStdin (void)
{
  enterprocedure("SetUpStdin");
  UsingFD(fileno(stdin), ReadStdin, NULL, NULL);
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/*
  xscope is really meant to look at one client at a time.  However,
  it can easily handle multiple clients and servers.  To do so,
  we need to have a pair of FDs: one for the client and one for the
  server for that client.  If either goes away, so does the other.
  We need to be able to identify the other FD of a pair, so that if
  we get input from one, we can write it to the other.
*/

static long clientNumber = 0;
struct fdinfo   FDinfo[StaticMaxFD];

void
SetUpPair(
    FD client,
    FD server)
{
  if (client >= 0)
    {
      clientNumber += 1;
      FDinfo[client].Server = false;
      FDinfo[client].pair = server;
      FDinfo[client].ClientNumber = clientNumber;
      FDinfo[client].bufcount = 0;
      FDinfo[client].buflimit = -1;
      FDinfo[client].bufdelivered = 0;
      if (server >= 0)
	{
	  FDinfo[server].Server = true;
	  FDinfo[server].pair = client;
	  FDinfo[server].ClientNumber = FDinfo[client].ClientNumber;
	  FDinfo[server].bufcount = 0;
	  FDinfo[server].buflimit = -1;
	  FDinfo[server].bufdelivered = 0;
	}
    }
  else if (server >= 0)
      {
	close(server);
	NotUsingFD(server);
      }
}

static void
ResetPair (
    FD client,
    FD server)
{
  if (client >= 0)
  {
    FDinfo[client].bufcount = 0;
    FDinfo[client].buflimit = -1;
    FDinfo[client].bufdelivered = 0;
  }
  if (server >= 0)
  {
    FDinfo[server].bufcount = 0;
    FDinfo[server].buflimit = -1;
    FDinfo[server].bufdelivered = 0;
  }
}

static void
CloseConnection (
    FD fd)
{
  debug(4,(stderr, "close %d and %d\n", fd, FDPair(fd)));
  ResetPair (ClientHalf(fd), ServerHalf(fd));
  StopClientConnection(ServerHalf(fd));
  StopServerConnection(ClientHalf(fd));

#ifdef USE_XTRANS
  _X11TransClose(GetXTransConnInfo(fd));
  _X11TransClose(GetXTransConnInfo(FDPair(fd)));
#else
  close(fd);
  close(FDPair(fd));
#endif
  NotUsingFD(fd);
  NotUsingFD(FDPair(fd));
  if (TerminateClose)
    exit (0);
}

/* ************************************************************ */

FD
FDPair (
    FD fd)
{
  return(FDinfo[fd].pair);
}

FD
ClientHalf (
    FD fd)
{
  if (FDinfo[fd].Server)
    return(FDinfo[fd].pair);
  return(fd);
}

FD
ServerHalf (
    FD fd)
{
  if (FDinfo[fd].Server)
    return(fd);
  return(FDinfo[fd].pair);
}

char *
ClientName (
    FD fd)
{
  static char name[12];

  if (clientNumber <= 1)
    return("");
  (void)sprintf(name, " %ld", FDinfo[fd].ClientNumber);
  return(name);
}

int
ClientNumber (
    FD fd)
{
    return FDinfo[fd].ClientNumber;
}

/* ********************************************** */
/*                                                */
/* ********************************************** */

/*
 * Write as much of the queued data as the receiver will accept
 * Block reads from the sender until the receiver gets all of the
 * data
 */
void
FlushFD (
    FD  fd)
{
  long    BytesToWrite = FDinfo[fd].bufcount - FDinfo[fd].bufstart;
  unsigned char	  *p = FDinfo[fd].buffer + FDinfo[fd].bufstart;
  int	  PeerFD;

  PeerFD = FDPair (fd);
  if (FDinfo[fd].buflimit >= 0)
  {
    if (FDinfo[fd].bufdelivered + BytesToWrite > FDinfo[fd].buflimit)
      BytesToWrite = FDinfo[fd].buflimit - FDinfo[fd].bufdelivered;
  }
  while (BytesToWrite > 0)
  {
    int     n1 = write (fd, (char *)p, (int)BytesToWrite);
    debug(4,(stderr, "write %d bytes\n", n1));
    if (n1 < 0)
    {
      if (errno != EWOULDBLOCK && errno != EAGAIN)
      {
	perror("Error on write");
	if (PeerFD >= 0)
	  CloseConnection(PeerFD);
	BytesToWrite = 0;
      }
      break;
    }
    else
    {
      FDinfo[fd].bufstart += n1;
      FDinfo[fd].bufdelivered += n1;
      BytesToWrite -= n1;
      p += n1;
    }
  }
  if (FDinfo[fd].bufstart == FDinfo[fd].bufcount)
  {
    if (PeerFD >= 0) {
      FD_CLR(PeerFD, &BlockedReadDescriptors);
    }
    FD_CLR(fd, &WriteDescriptors);
    FDinfo[fd].bufcount = FDinfo[fd].bufstart = 0;
  }
  else
  {
    if (PeerFD >= 0) {
      FD_SET(PeerFD, &BlockedReadDescriptors);
    }
    if (FDinfo[fd].buflimit != FDinfo[fd].bufdelivered) {
      FD_SET(PeerFD, &WriteDescriptors);
    }
  }
}

/* when we get data from a client, we read it in, copy it to the
   server for this client, then dump it to the client. Note, we don't
   have to have a server, if there isn't one. */

static void
DataFromClient (
    FD fd)
{
  long    n;
  FD ServerFD;

  Verbose = XVerbose;
  enterprocedure("DataFromClient");
  ServerFD = FDPair(fd);
  if (ServerFD < 0)
    {
      ServerFD = ConnectToServer(false);
      if (ServerFD < 0)
      {
	CloseConnection(fd);
	return;
      }
      SetUpPair(fd, ServerFD);
    }

  n = read(fd, FDinfo[ServerFD].buffer, BUFFER_SIZE);
  debug(4,(stderr, "read %ld bytes from Client%s\n", n, ClientName(fd)));
  if (n < 0)
    {
      PrintTime();
      perror("Client --> read error:");
      CloseConnection(fd);
      return;
    }
  if (n == 0)
    {
      PrintTime();
      if (Verbose >= 0)
	fprintf(stdout, "Client%s --> EOF\n", ClientName(fd));
      CloseConnection(fd);
      return;
    }

  FDinfo[ServerFD].bufcount = n;
  FDinfo[ServerFD].bufstart = 0;

  FlushFD (ServerFD);
  /* also report the bytes to standard out */
  ReportFromClient(fd, FDinfo[ServerFD].buffer, n);
}

/* ********************************************** */
/*                                                */
/* ********************************************** */

/* similar situation for the server, but note that if there is no client,
   we close the connection down -- don't need a server with no client. */

static void
DataFromServer (
    FD fd)
{
  long    n;
  FD ClientFD;

  Verbose = XVerbose;
  ClientFD = FDPair(fd);
  if (ClientFD < 0)
    {
      CloseConnection(fd);
      return;
    }

  enterprocedure("DataFromServer");
  n = read(fd, (char *)FDinfo[ClientFD].buffer, BUFFER_SIZE);
  debug(4,(stderr, "read %ld bytes from Server%s\n", n, ClientName(fd)));
  if (n < 0)
    {
      PrintTime();
      perror("read error <- Server");
      CloseConnection(fd);
      return;
    }
  if (n == 0)
    {
      PrintTime();
      if (Verbose >= 0)
	fprintf(stdout, "EOF <-- Server%s\n", ClientName(fd));
      CloseConnection(fd);
      return;
    }

  FDinfo[ClientFD].bufcount = n;
  FDinfo[ClientFD].bufstart = 0;
  FlushFD (ClientFD);

  /* also report the bytes to standard out */
  ReportFromServer(fd, FDinfo[ClientFD].buffer, n);
}



/* ************************************************************ */
/*								*/
/*     Create New Connection to a client program and to Server  */
/*								*/
/* ************************************************************ */


void
NewConnection (
    FD fd)
{
  FD ClientFD = ConnectToClient(fd);
  FD ServerFD = ConnectToServer(true);

  SetUpPair(ClientFD, ServerFD);
}


/* ************************************************************ */

static FD
ConnectToClient (
    FD ConnectionSocket)
{
  FD ClientFD;
  XtransConnInfo trans_conn = NULL;
#ifdef USE_XTRANS
  XtransConnInfo listen_conn;
  int status;
#else
  struct sockaddr_in  from;
  int    len = sizeof (from);
  int    ON = 1 /* used in ioctl */ ;
#endif

  enterprocedure("ConnectToClient");

#ifdef USE_XTRANS
  listen_conn = GetXTransConnInfo(ConnectionSocket);
  if ((trans_conn = _X11TransAccept (listen_conn, &status)) == NULL) {
      debug(4,(stderr, "Failed to accept connection\n"));
      return -1;
  }
  _X11TransSetOption(trans_conn, TRANS_NONBLOCKING, 1);
  ClientFD = _X11TransGetConnectionNumber(trans_conn);  
#else
  ClientFD = AcceptConnection(ConnectionSocket);
#endif
  debug(4,(stderr, "Connect To Client: FD %d\n", ClientFD));
  if (ClientFD < 0 && errno == EWOULDBLOCK)
    {
      debug(4,(stderr, "Almost blocked accepting FD %d\n", ClientFD));
      panic("Can't connect to Client");
    }
  if (ClientFD < 0)
    {
      debug(4,(stderr, "NewConnection: error %d\n", errno));
      panic("Can't connect to Client");
    }

  UsingFD(ClientFD, DataFromClient, FlushFD, trans_conn);
#ifndef USE_XTRANS
  ioctl(ClientFD, FIOCLEX, 0);
  ioctl(ClientFD, FIONBIO, &ON);
#endif
  StartClientConnection(ClientFD);
  return(ClientFD);
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */



FD
ConnectToServer (
    Boolean report)
{
  FD ServerFD;
  XtransConnInfo trans_conn = NULL;   /* transport connection object */
  short port;

  enterprocedure("ConnectToServer");

  port = GetServerport ();

  if (port == ScopePort &&
      ((ServerHostName[0] == '\0') || strcmp(ServerHostName, ScopeHost) == 0))
    {
      char error_message[100];
      sprintf(error_message, "Trying to attach to myself: %s,%d\n",
	      ServerHostName, port);
      panic(error_message);
    }

  ServerFD = MakeConnection (ServerHostName, port, report, &trans_conn);

  debug(4,(stderr, "Connect To Server: FD %d\n", ServerFD));
  if (ServerFD >= 0)
    {
      UsingFD(ServerFD, DataFromServer, FlushFD, trans_conn);
      StartServerConnection(ServerFD);
    }
  return(ServerFD);
}
