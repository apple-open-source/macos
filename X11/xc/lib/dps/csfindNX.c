/*
 * csfindNX.c  -- DPSCAP client Xlib extension hookup
 *
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
/* $XFree86: xc/lib/dps/csfindNX.c,v 1.7 2001/10/28 03:32:42 tsi Exp $ */

#include <sys/param.h>				/* for MAXHOSTNAMELEN */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>		/* getuid() */
#include <string.h>
#include <pwd.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>

#include "csfindNX.h"
#include "dpsNXprops.h"
#include "Xlibnet.h"
#include "DPS/dpsXclient.h"
#include "DPS/dpsNXargs.h"
#include <DPS/XDPSlib.h>

#include "dpsassert.h"
#include "cslibint.h"

/* ---Defines--- */

#define CANTSTARTAGENT "Unable to start agent.\n"

/* ---Types--- */

typedef struct {
  Window id;
  int willingness;
} Agent, *AgentIdList;

/* ---Globals--- */

static char *XDPSLNXHost = NULL;
static int XDPSLNXPort = 0;	/* default port to be used */
static int XDPSLNXTrans = XDPSNX_TRANS_UNIX;
static int gXDPSNXLaunchedAgentTrans = XDPSNX_USE_BEST;
static int gXDPSNXLaunchedAgentPort = XDPSNX_USE_BEST;
static char *gXDPSNXExecObj = XDPSNX_DEFAULT_EXEC_NAME; /* NX object file */
static char **gXDPSNXExecArgs = NULL; /* the args for gXDPSNXExecObj */
static Bool gWasAgentSet = False;
static Bool gXDPSNXAutoLaunch = False;
unsigned char gXDPSNXErrorCode;
extern int gNXSndBufSize;  /* Output buffer size, zero means default */

static char *getHomeDir(char *);

/* ---Private Functions--- */

static int
TmpErrorHandler(
     Display *dpy,
     XErrorEvent *err)
{
  gXDPSNXErrorCode = err->error_code;
  return 0;
}


static int
GetProperty(
     Display *dpy,
     Window w,
     Atom prop,
     Atom type,
     unsigned long *nitems,	/* IN: if non-null then RETURN val */
     char **data)		/* RETURN */
{
  long largest_len = ((((unsigned long) -1) >> 1) / 4);
  Atom actual_type;
  int actual_format;
  unsigned long actual_number;
  unsigned long remaining;

  if (type == None) return(!Success); /* We need to have a type */
  if ((XGetWindowProperty(dpy, w, prop, 0, largest_len, False, type, 
			 &actual_type, &actual_format, &actual_number,
			 &remaining, (unsigned char **) data) == Success)
      && (actual_type == type)) {
    if (nitems != NULL) *nitems = actual_number;
    return(Success);
  } else {
    if (*nitems != 0) {		/* if data returned, clean up */
      XFree(*data);
      *data = NULL;		/* is this necessary? */
      *nitems = 0;
    }
    return(!Success);
  }
}


/*
  GetAgentIdList returns a list of agents advertising on the passed display (dpy),
  and it returns the number of agents in the list in the parameter nAgents.
  The returned list is sorted in most to least willing order.
  GetAgentIdList assumes that it has exclusive access to the server associated with
  dpy (ie. it assumes that the caller has done an XGrabServer).
  */
static AgentIdList
GetAgentIdList(
     Display *dpy,
     unsigned long *nAgents)	/* RETURN: number of agents in list */
{
  Atom serverListAtom = XInternAtom(dpy, XDPSNX_BILLBOARD_PROP, True);
  Window  *agents = NULL;
  AgentIdList agentList = NULL;
  int (*oldErrorHandler)(Display *, XErrorEvent *) = 0; /* the current error handler */
  unsigned long i, current;

  if (serverListAtom == None) { /* Servers registered on dpy */
    goto failed;
  }

  XSync(dpy, False);		/* force processing of pending requests */

  if (GetProperty(dpy, RootWindow(dpy, DefaultScreen(dpy)),
		  serverListAtom, XA_WINDOW, nAgents,
		  (char **) &agents) != Success) {
    goto failed;
  }

  if ((agentList = (AgentIdList)
       Xcalloc(*nAgents, (unsigned) sizeof(Agent))) == NULL)
    goto failed;

  /* let's go through the list and figure out which servers  are okay */
  oldErrorHandler = XSetErrorHandler(TmpErrorHandler);    
  current = 0;
  for (i=0; i < *nAgents; i++) {
    unsigned long len;
    int *agentWillingness;
    unsigned long k;

    for (k=0; k < i; k++) /* Search for duplicate ids */
      if (agents[i] == agents[k]) { /* uh oh... */
	agents[i] = None;
	break;
      }
    if (k == i)	{		/* No duplicate ids */
      if (GetProperty(dpy, agents[i],
		      XInternAtom(dpy, XDPSNX_WILLINGNESS_PROP, True),
		      XA_INTEGER, &len, (char **) &agentWillingness)
	  != Success) {
	/* Assume that Agent i is dead... */
	/* reap the agent */
	agents[i] = None;
	gXDPSNXErrorCode = None;
      } else {
	unsigned long j = 0;
	
	/* insert the agents into agentList in "most to least willing" order */
	while ((j < current) && (agentList[j].willingness > *agentWillingness))
	  j++;
	if (j < current)
	  (void) bcopy((char *) &agentList[j], (char *) &agentList[j+1],
		       sizeof(Agent) * (*nAgents - j - 1));
	agents[current] = agents[i];
	agentList[j].id = agents[current++];
	agentList[j].willingness = *agentWillingness;
	XFree(agentWillingness);
      }
    }
  }
  (void) XSetErrorHandler(oldErrorHandler);
  oldErrorHandler = NULL;
  if (*nAgents != current) {	/* agent list changed */
    if (current > 0) {		/* are there living ones? */
      *nAgents = current;
      /* write the list  back out onto the root window */
      (void) XChangeProperty(dpy, RootWindow(dpy, DefaultScreen(dpy)),
		      serverListAtom, XA_WINDOW, 32,
		      PropModeReplace, (unsigned char *) agents, *nAgents);
    } else {
      (void) XDeleteProperty(dpy, RootWindow(dpy, DefaultScreen(dpy)),
			     serverListAtom);
      goto failed;
    }
  }
  (void) XFree(agents);
  return(agentList);

 failed:
  if (agents != NULL) XFree(agents);
  if (agentList != NULL) XFree(agentList);
  if (oldErrorHandler != NULL) (void) XSetErrorHandler(oldErrorHandler);
  *nAgents = 0;
  return(NULL);
}


static int
XDPSNXOnDisplay(
     Display *dpy,
     char *licenseMethod,
     char **host,
     int *transport,
     int *port)
{
  unsigned long nAgents = 0;
  AgentIdList agentList = NULL;
  Bool match = False;
  unsigned long i = 0;
  int status = !Success;

#ifdef DPSLNKL
  extern unsigned ANXKFunc();
#endif /* DPSLNKL */

  (void) XGrabServer(dpy);	/* Can we do this later? */
  if ((agentList = GetAgentIdList(dpy, &nAgents)) == NULL)
    goto cleanup;
  /* NOTE: agentList was sorted most to least willing by GetAgentIdList */
  if (agentList[i].willingness <= 0) { /* Is there a willing agent? */
    DPSWarnProc(NULL, "Found agent(s) for display, but none willing to accept connections.\n");
    goto cleanup;
  }
  
#ifdef DPSLNKL
  /* Masterkey bypass code */
  if (ANXKFunc() != 0) {
    /* We have a willing agent and the client has masterkeys so... */
    match = True;
  } else			/* Search for appropriate agent */
#endif /* DPSLNKL */
  {
    /* need to get licensing info from dpy */
    Atom desiredLicenseMethod, openLicenseMethod;
    char openLicenseString[256];

    (void) sprintf(openLicenseString, "%s:%d",
                   LICENSE_METHOD_OPEN,
		   OPEN_LICENSE_VERSION);
    openLicenseMethod = XInternAtom(dpy, openLicenseString, True);

    if (licenseMethod != NULL)
      desiredLicenseMethod = XInternAtom(dpy, licenseMethod, True);
    else
      desiredLicenseMethod = None;

    if ((desiredLicenseMethod != None) || (openLicenseMethod != None)) {
      for (i=0;
	   (i < nAgents) && (agentList[i].willingness > 0) && (match == False);
	   i++) {
	Atom *licenseMethods = NULL;
	unsigned long nMethods;
	unsigned long j;
	
	if (GetProperty(dpy, agentList[i].id,
			XInternAtom(dpy, XDPSNX_LICENSE_METHOD_PROP, True),
			XA_ATOM,
			&nMethods, (char **) &licenseMethods) != Success)
	  goto cleanup;
	
	/*
	  Check to see if the agent supports either our desired license method or
	  if it is an open service.
	 */
	j = 0;
	while((j < nMethods)
	      && (licenseMethods[j] != desiredLicenseMethod)
	      && (licenseMethods[j] != openLicenseMethod)) j++;
	if (j < nMethods) {	/* We found one */
	  match = True;
	  break;
	}
	(void) XFree(licenseMethods);
      }
    }
  }
  
  if (match) {			/* We had a match on license method */
    TransportInfo *transInfo;
    
    if (GetProperty(dpy, agentList[i].id,
		    XInternAtom(dpy, XDPSNX_TRANSPORT_INFO_PROP, True),
		    XA_INTEGER, NULL, (char **) &transInfo) != Success) {
      goto cleanup;
    } else {			/* We got one! */
      *transport = transInfo->transport;
      *port = transInfo->port;
      match = True;
      XFree(transInfo);
    }
    if (GetProperty(dpy, agentList[i].id,
		    XInternAtom(dpy, XDPSNX_HOST_NAME_PROP, True), XA_STRING,
		    NULL, (char **) host) == Success) {
      status = Success;
      /* 
       * If transport is TCP, but we are on the same host as the agent then 
       * trade-up to the more efficient UNIX transport...
       */
      if (*transport == XDPSNX_TRANS_TCP) {
        char hostname[MAXHOSTNAMELEN];
	(void) N_XGetHostname(hostname, MAXHOSTNAMELEN);
        if (strcmp(hostname, *host) == 0)
	  *transport = XDPSNX_TRANS_UNIX;
      }
    }
  }
 /*
  * Exit Clauses: status inited to FAILURE.  Therefore cleanup "assumes"
  * a failure unless noted otherwise.
  */
 cleanup:
  (void) XUngrabServer(dpy);
  (void) XDPSLFlush(dpy);		/* Flush the ungrab */
  if (agentList != NULL) XFree(agentList);
  return(status);
}


static int
ParseAgentString(
     char *string,
     char **hostname,		/* RETURN */
     int *transport,		/* RETURN */
     int *port)			/* RETURN */
{
  int dnet = 0;
  Bool transportSpecified = False;
  char namebuf[255];
  char *lastp, *p;
  
  (void) strncpy(namebuf, string, strlen(string)+1);
  p = &namebuf[0];
  /*
   * Step 1, find the hostname.  This is delimited by a required colon.
   */
  for (lastp = p; *p && *p != ':'; p++);
  if (!*p) return(!Success);	/* There must be a colon */
  
  if (*(p+1) == ':') {
    dnet++;
    *(p++) = '\0';
  }
  *(p++) = '\0';

  /* 
   * Step 2, get the port number.  It follows the colon(s)
   */ 
  if (*p == '\0')		/* No port number specified... */
    return(!Success);

  *port = atoi(p);
  
  /*
   * Step 3, transport?
   */
  if (namebuf[0] == '\0') {	/* no transport/hostname specified... */
    if (dnet)
      (void) strcpy(namebuf, "0");
    else {			/* no hostname, so must be UNIX */
      *hostname = NULL;
      *transport = XDPSNX_TRANS_UNIX;
      return(Success);
    }
  } else {
    /* find the delimiting '/' */
    for (p = &namebuf[0]; *p && *p != '/'; p++);
    if (*p == '/') {
      transportSpecified = True;
      *p = '\0';
      p++;
    } else			/* no transport specified */
      p = &namebuf[0];
    if ((*hostname = (char *) Xmalloc(strlen(p))) == NULL)
      return(!Success);		/* can't alloc space for hostname */
    (void) strcpy(*hostname, p);

    /* identify protocol */
    if (dnet)
      *transport = XDPSNX_TRANS_DECNET;
    else if (transportSpecified) {
      if (strcmp(namebuf, "unix") == 0)
        *transport = XDPSNX_TRANS_UNIX;
      else 			/* assume tcp */
        *transport = XDPSNX_TRANS_TCP;
    } else 
      *transport = XDPSNX_TRANS_TCP;
  }
  return(Success);
}

static int
FindXDPSNXInXrmDatabase(
     Display *dpy,
     char **host,
     int *transport,
     int *port)
{
  XrmDatabase rDB = NULL;	/* for merged database */
  XrmDatabase serverDB;
  char filenamebuf[1024];
  char *filename = &filenamebuf[0];
  char *env, *str_type;
  char name[255];
  XrmValue value;
  int retVal = !Success;

  XrmInitialize();
  (void) strcpy(name, "/usr/lib/X11/app-defaults/");
  (void) strcat(name, XDPSNX_X_CLASS_NAME);
  
  /* try to get application defaults file, if there is any */
  XrmMergeDatabases(XrmGetFileDatabase(name), &rDB);

  /* try to merge the server defaults.  if not defined then use .Xdefaults */
  if (XResourceManagerString(dpy) != NULL) {
    serverDB = XrmGetStringDatabase(XResourceManagerString(dpy));
  } else {			/* use the .Xdefaults */
    (void) getHomeDir(filename);
    (void) strcat(filename, "/.Xdefaults");
    
    serverDB = XrmGetFileDatabase(filename);
  }
  XrmMergeDatabases(serverDB, &rDB);
 
  /* try the XENVIRONMENT file, or if not defined, then .Xdefaults */
  if ((env = getenv("XENVIRONMENT")) == NULL) {
    int len;
    env = getHomeDir(filename);
    (void) strcat(filename, "/.Xdefaults-");
    len = strlen(env);
    (void) gethostname(env+len, 1024-len);
  }
  XrmMergeDatabases(XrmGetFileDatabase(env), &rDB);

  /* Now that the database is built, try to extract the values we want. */

  if (XrmGetResource(rDB, XDPSNX_X_RESOURCE, XDPSNX_X_CLASS_NAME, &str_type,
		     &value) == True) {
    retVal = ParseAgentString((char *) value.addr, host, transport, port);
  }
  (void) XrmDestroyDatabase(rDB);
  return(retVal);
}


static char *
getHomeDir(char *dest)
{
  register char *ptr;

  if ((ptr = getenv("HOME")) != NULL) {
    (void) strcpy(dest, ptr);
  } else {
    struct passwd *pw;

    if ((ptr = getenv("USER")) != NULL) {
      pw = getpwnam(ptr);
    } else {
      pw = getpwuid(getuid());
    }
    if (pw) {
      (void) strcpy(dest, pw->pw_dir);
    } else {
      *dest = ' ';
    }
  }
  return (dest);
}



/* ---Public Functions--- */

int gForceLaunchHack = 0;   /* Undocumented way to force autolaunch */

XDPSNXFindNXResult
XDPSNXFindNX(
     Display *dpy,
     char *licenseMethod,
     char **host,
     int *transport,
     int *port)
{
  char *agentenv;
  
  if (gForceLaunchHack)
    return(findnx_not_found);

  if (gWasAgentSet) {			/* check if client set up host */
    *host = XDPSLNXHost;
    *transport = XDPSLNXTrans;
    *port = XDPSLNXPort;
    return(findnx_found);
  /* check DPSNXHOST environment variable */
  } else if ((agentenv = getenv(AGENT_ENV_VAR)) != NULL) { 
    int status = ParseAgentString(agentenv, host, transport, port);
    if (status != Success) {
      DPSWarnProc((DPSContext) NULL, "Illegal syntax for DPSNXHOST");
      return(findnx_error);
    } else return(findnx_found);
  /* check advertisements... */
  } else if (XDPSNXOnDisplay(dpy, licenseMethod, host, transport, port) ==
	     Success) {
    return(findnx_found);
  /* check XrmDatabase */
  } else if (FindXDPSNXInXrmDatabase(dpy, host, transport, port) == Success) {
    return(findnx_found);
  }
  /* Nada */
  return(findnx_not_found);
}


Status
XDPSNXSetClientArg(int arg, void *value)
{
  Display *dpy;
  
  if (arg == XDPSNX_AGENT) {
    gWasAgentSet = True;
    return(ParseAgentString(value,
			    &XDPSLNXHost, &XDPSLNXTrans, &XDPSLNXPort));
  }
  else if (arg == XDPSNX_EXEC_FILE) {
    if ((gXDPSNXExecObj = Xmalloc(strlen((char *) value) + 1)) == NULL) {
      return(!Success);
    }
    gXDPSNXExecObj = strcpy(gXDPSNXExecObj, (char *) value);
  } else if (arg == XDPSNX_EXEC_ARGS) {
    int i;
    char **cpp, **execInfo;

    execInfo = (char **) value;
    for(cpp = execInfo, i = 1; *cpp != NULL; i++, cpp++); /* count */
    gXDPSNXExecArgs = (char **) Xcalloc(i, sizeof(char *));
    if (gXDPSNXExecArgs == NULL) return(!Success);
    for(cpp = gXDPSNXExecArgs; *execInfo != NULL;
	execInfo++, cpp++) {
      /* copy each entry */
      if ((*cpp = Xmalloc(strlen(*execInfo) + 1)) == NULL)
	return(!Success);
      *cpp = strcpy(*cpp, *execInfo);
    }
  } else if (arg == XDPSNX_AUTO_LAUNCH) {
    gXDPSNXAutoLaunch = (Bool)(long) value;
  } else if (arg == XDPSNX_LAUNCHED_AGENT_TRANS) {
    gXDPSNXLaunchedAgentTrans = (long) value;
  } else if (arg == XDPSNX_LAUNCHED_AGENT_PORT) {
    gXDPSNXLaunchedAgentPort = (long) value;
  } else if (arg == XDPSNX_REQUEST_XSYNC) {
    dpy = (Display *) value;
    if (dpy == (Display *)NULL)
        return(Success);
    XDPSLSetSyncMask(dpy, DPSCAP_SYNCMASK_SYNC);
  } else if (arg == XDPSNX_REQUEST_RECONCILE) {
    dpy = (Display *) value;
    if (dpy == (Display *)NULL)
        return(Success);
    XDPSLSetSyncMask(dpy, DPSCAP_SYNCMASK_RECONCILE);
  } else if (arg == XDPSNX_REQUEST_BUFFER) {
    dpy = (Display *) value;
    if (dpy == (Display *)NULL)
        return(Success);
    XDPSLSetSyncMask(dpy, DPSCAP_SYNCMASK_BUFFER);
  } else if (arg == XDPSNX_GC_UPDATES_SLOW) {
    dpy = (Display *) value;
    if (dpy == (Display *)NULL)
        return(Success);
    XDPSLSetGCFlushMode(dpy, XDPSNX_GC_UPDATES_SLOW);
  } else if (arg == XDPSNX_GC_UPDATES_FAST) {
    dpy = (Display *) value;
    if (dpy == (Display *)NULL)
        return(Success);
    XDPSLSetGCFlushMode(dpy, XDPSNX_GC_UPDATES_FAST);
  } else if (arg == XDPSNX_SEND_BUF_SIZE) {
    int i = (long)value;
    if (i >= 4096 && i <= 65536) gNXSndBufSize = i;
  }
  return(Success);
}


void
XDPSGetNXArg(int arg, void **value)
{
  static char agentBuffer[255];
  
  if (arg == XDPSNX_AGENT) {
    switch(XDPSLNXTrans) {
    case XDPSNX_TRANS_UNIX:
      (void) sprintf(agentBuffer, "unix/");
      break;
    case XDPSNX_TRANS_TCP:
      (void) sprintf(agentBuffer, "tcp/");
      break;
    case XDPSNX_TRANS_DECNET:
      (void) sprintf(agentBuffer, "decnet/");
      break;
    default:
      DPSWarnProc(NULL, "Unknown transport passed to XDPSGetNXArg ignored.\n");
      agentBuffer[0] = '\0';
      break;
    }
    (void) strcat(agentBuffer, XDPSLNXHost);
    (void) strcat(agentBuffer,
		  (XDPSLNXTrans == XDPSNX_TRANS_DECNET ? "::" : ":"));
    (void) sprintf(&agentBuffer[strlen(agentBuffer)], "%d", XDPSLNXPort);
    *value = (void *) agentBuffer;
  }
  else if (arg == XDPSNX_EXEC_FILE) {
    *value = (void *) gXDPSNXExecObj;
  } else if (arg == XDPSNX_EXEC_ARGS) {
    *value = (void *) gXDPSNXExecArgs;
  } else if (arg == XDPSNX_AUTO_LAUNCH) {
    *value = (void *) (long)gXDPSNXAutoLaunch;
  } else if (arg == XDPSNX_LAUNCHED_AGENT_TRANS) {
    *value = (void *) (long)gXDPSNXLaunchedAgentTrans;
  } else if (arg == XDPSNX_LAUNCHED_AGENT_PORT) {
    *value = (void *) (long)gXDPSNXLaunchedAgentPort;
  }
}
