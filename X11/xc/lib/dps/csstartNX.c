/*
  csstartNX.c

 * (c) Copyright 1992-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */
/* $XFree86: xc/lib/dps/csstartNX.c,v 1.7 2002/05/31 18:45:48 dawes Exp $ */

#include <sys/param.h>
#include <X11/X.h>
#include <X11/Xlibint.h>
#include <sys/wait.h>
#include <DPS/dpsNXargs.h>
#include <sys/socket.h>
#include <errno.h>
#include <X11/Xos.h>

#include "DPSCAPproto.h"
#include "Xlibnet.h"            /* New for R5, delete for R4 */
#include "dpsassert.h"
#include "csfindNX.h"
#include "csstartNX.h"

/* ---Defines--- */

#include <stddef.h>
  
#define DOZETIME 1              /* time to wait for agent to start up (sec) */

#define BASE_TCP_PORT CSDPSPORT

#ifndef CSDPSMAXPORT
#define CSDPSMAXPORT 16
#endif

#ifndef SO_REUSEADDR
#define SO_REUSEADDR 1
#endif

/* ---Globals--- */

pid_t gSecretAgentPID = 0;	/* PID of launched agent *Shh!* Not public! */

/* ---Private Functions--- */

static int
TryTCP(void)
{
    struct sockaddr_in insock;
    int request;
    unsigned short port, startPort = 0;
    struct servent *serventInfo;
    int okay;

#ifndef ultrix
    /* Ultrix has a nasty bug in getservbyname().  If the name passed
       to it doesn't exist in the services list it will seg. fault...
       * sigh *  */
    if ((serventInfo = getservbyname(DPS_NX_SERV_NAME,
    					(char *) 0)) != 0)
        if (strcmp("tcp", serventInfo->s_proto) == 0) {
	  startPort = ntohs(serventInfo->s_port);
	}
    /* So, for Ultrix we just default to the default default port :-) */ 
#endif /* ultrix */
    if (startPort == 0) startPort = BASE_TCP_PORT;
    if ((request = socket (AF_INET, SOCK_STREAM, 0)) < 0) 
    {
	DPSWarnProc(NULL, "Creating TCP socket while recommending port\n");
	return -1;
    } 
#ifdef SO_REUSEADDR
    /* Necesary to restart the server without a reboot */
    {
	int one = 1;
	setsockopt(request, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(int));
    }
    /* What the hell is all this?  I'll tell you.  We don't know
       a prioi what port is free, so we try to bind to each
       in sequence and return the one that works. */
#if !defined(AIXV3)
    {
      struct linger lingere;
      
      lingere.l_onoff = 0;		/* off */
      lingere.l_linger = 0;		/* don't */
      if(setsockopt(request, SOL_SOCKET, SO_LINGER, (char *)&lingere,
        sizeof(struct linger)) != 0)
      DPSWarnProc(NULL,
		  "Couldn't set TCP SO_DONTLINGER while recommending port.");
    }
#endif /* AIXV3 */
#endif /* SO_REUSEADDR */
    bzero((char *)&insock, sizeof (insock));
    insock.sin_family = AF_INET;
    insock.sin_addr.s_addr = htonl(INADDR_ANY);
    okay = 0;
    
    for (port = startPort; (int) port < (int) startPort + CSDPSMAXPORT; port++)
    {
    int result;
    insock.sin_port = htons(port);
    
    errno = 0;
    result = bind(request, (struct sockaddr *) &insock, sizeof (insock));
    if (result < 0)
        {
	if (errno != EADDRINUSE)
	    {
	    DPSWarnProc(NULL, "Binding TCP socket while recommending port.\n");
	    close(request);
	    return -1;
	    }
	continue;
	}
    else
        {
        /* We have a good port number */
        okay = 1;
	break;
	}
    }
    close(request);
    return (okay) ? port : -1;
}

/* ---Functions--- */

int
XDPSNXRecommendPort(int transport)
{
    int ret;
    
    switch (transport)
        {
	case XDPSNX_TRANS_UNIX:
	    /* If the TCP socket exists, we just assume the UNIX one
	       is there too.  FALL THRU! */
	case XDPSNX_TRANS_TCP: /* TCP */
	    ret = TryTCP();
	    break;
	default: ret = -1;
	}
    return(ret);
}

int
StartXDPSNX(char **additionalArgs)
{
  char **args, **cpp;
  pid_t childPid;
  int argc = 1;			/* 1, args[0]:=path, args[1]:=null */
  int i = 0;
  int status = Success;		/* assume we'll succeed */
  char *execObj, **execArgs;

  (void) XDPSGetNXArg(XDPSNX_EXEC_FILE, (void **) &execObj);
  if (execObj == 0) return (!Success);

  /* Create the argv list for the execl() call */
  (void) XDPSGetNXArg(XDPSNX_EXEC_ARGS, (void **) &execArgs);
  if (execArgs != 0)
    for(cpp = execArgs; *cpp != 0; cpp++, argc++); /* count args. */
  if (additionalArgs != 0)	/* add on the add-on args. */
    for(cpp = additionalArgs; *cpp != 0; cpp++, argc++); 

  args = (char **) Xmalloc(sizeof(char *) * (argc+1));
  if (args == 0)
    return(!Success);
  args[argc] = 0;		/* cap end of args */
  args[i++] = execObj;
  if (additionalArgs != 0)
    for(cpp = additionalArgs; *cpp != 0; cpp++, i++) args[i] = *cpp;
  if (execArgs != 0)
    for(cpp = execArgs; *cpp != 0; cpp++, i++) args[i] = *cpp;

  /* now try to start up the agent... */
  if ((childPid = fork()) != -1) {
    if (childPid == 0) {	/* Child process */
#ifndef __UNIXOS2__
      if (setsid() < 0)
        DPSWarnProc(NULL, "Agent unable to create session.  Continuing...\n");
#endif

      /* Try to start the agent */
      if (execvp(args[0], args) < 0) { /* Error!! */
	exit(1);		/* This is OKAY, we're the child here */
      }
      /* SHOULD NEVER REACH HERE */
    } else {			/* Parent (NX Client) */
      (void) sleep(DOZETIME);
      /* if decmips, pray that we hesitate long enough for the child... */
      /* Check on child (NX Agent) */
      if (waitpid(childPid, NULL, WNOHANG) != 0) {
	/* Server terminated or stopped; don't care, result is same... */
	status = !Success;
      } else {			/* we think the agent started okay */
	gSecretAgentPID = childPid; /* set secret global */
      }
    }
  } else {			/* Error in fork */
    status = !Success;
  }
  if (args != 0) (void) XFree(args);
  return(status);
}
