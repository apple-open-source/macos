/*
 * csopendi.c -- open connection to CSDPS agent
 *
 * (c) Copyright 1990-1994 Adobe Systems Incorporated.
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
 * Portions Copyright    Massachusetts Institute of Technology    1985, 1986
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *  
 * Author:  Adobe Systems Incorporated and MIT X Consortium
 */
/* $XFree86: xc/lib/dps/csopendi.c,v 1.3 2001/10/28 03:32:43 tsi Exp $ */
 
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>		/* for MAXHOSTNAMELEN */
#define NEED_EVENTS
#include <X11/Xlibint.h>
#include <X11/Xos.h>
#include "cslibint.h"
#ifdef XXX
#include <X11/Xauth.h>
#include <X11/Xatom.h>

extern int _Xdebug;
extern Display *_XHeadOfDisplayList;

#ifndef lint
static int lock;	/* get rid of ifdefs when locking implemented */
#endif

#endif /* XXX */

#include <X11/X.h>
#include <X11/Xproto.h>

#include "DPSCAPClient.h"
#include <DPS/dpsXclient.h>
#include <DPS/dpsNXargs.h>
#include "dpsassert.h"
#include "dpsNXprops.h"
#include "csfindNX.h"
#include "csstartNX.h"

#ifdef DPSLNKL
#include "dpslnkl.inc"
#endif /* DPSLNKL */


/* +++ Someday make this common with XDPS.c version */
#define DPY_NUMBER(dpy) ((dpy)->fd)

static xReq _dummy_request = {
	0, 0, 0
};

static void OutOfMemory (Display *);

#ifdef XXX
/*
 * First, a routine for setting authorization data
 */
static int xauth_namelen = 0;
static char *xauth_name = NULL;	 /* NULL means use default mechanism */
static int xauth_datalen = 0;
static char *xauth_data = NULL;	 /* NULL means get default data */

void XSetAuthorization (char *name, int namelen, char *data, int datalen)
{
    char *tmpname, *tmpdata;

    if (xauth_name) Xfree (xauth_name);	 /* free any existing data */
    if (xauth_data) Xfree (xauth_data);

    xauth_name = xauth_data = NULL;	/* mark it no longer valid */
    xauth_namelen = xauth_datalen = 0;

    if (namelen < 0) namelen = 0;	/* check for bogus inputs */
    if (datalen < 0) datalen = 0;	/* maybe should return? */

    if (namelen > 0)  {			/* try to allocate space */
	tmpname = Xmalloc ((unsigned) namelen);
	if (!tmpname) return;
	bcopy (name, tmpname, namelen);
    } else {
	tmpname = NULL;
    }

    if (datalen > 0)  {
	tmpdata = Xmalloc ((unsigned) datalen);
	if (!tmpdata) {
	    if (tmpname) (void) Xfree (tmpname);
	    return;
	}
	bcopy (data, tmpdata, datalen);
    } else {
	tmpdata = NULL;
    }

    xauth_name = tmpname;		/* and store the suckers */
    xauth_namelen = namelen;
    xauth_data = tmpdata;
    xauth_datalen = datalen;
    return;
}

#endif /* XXX */

/* 
 * Connects to a server, creates a Display object and returns a pointer to
 * the newly created Display back to the caller.
 */
XExtData * 
DPSCAPOpenAgent(Display *dpy, char *trueDisplayName)
{
	register Display *agent;
	char *agentHost = (char *)NULL;
	register int i;
	char display_name[256];	        /* pointer to display name */
	char licMethBuf[256];
	char *licMeth = licMethBuf;
	char *fullname = NULL;
	int idisplay;
	char *server_addr = NULL;
	int server_addrlen = 0;
	int conn_family;
	int transport, port;
	XExtData *ext;
	DPSCAPData my;
	char hostname[MAXHOSTNAMELEN];

/*
 * Find an agent to talk to.
 */
#ifdef DPSLNKL
	extern unsigned ANXPFunc();
#ifdef PSUSEPN
	(void) sprintf(licMeth, "%s%s:%d",
		       LICENSE_METHOD_PREFIX,
		       ANXVENDOR, /* From dpslnkl.inc */
		       ANXPFunc());
#else /* PSUSEPN */
	(void) sprintf(licMeth, "%s%s",
                       LICENSE_METHOD_PREFIX,
                       ANXVENDOR); /* From dpslnkl.inc */
#endif /* PSUSEPN */	
#else /* DPSLNKL */
	licMeth = NULL;		/* We want an open service */
#endif /* DPSLNKL */	
	(void) N_XGetHostname(hostname, MAXHOSTNAMELEN);
	switch(XDPSNXFindNX(dpy, licMeth, &agentHost, &transport, &port)) {
	case findnx_not_found: {
	    /* try to start-up an NX? */
	    Bool autoLaunch;
    
	    XDPSGetNXArg(XDPSNX_AUTO_LAUNCH, (void **) &autoLaunch);
	    if (autoLaunch == True) {
	    int requestedTrans;
	    int requestedPort = 0;
	    char **args = NULL;
	    char *additionalArgs[2];
	    char transportArg[256];
    
	    (void) DPSWarnProc(NULL, "Auto-launching DPS NX agent.");
	    XDPSGetNXArg(XDPSNX_LAUNCHED_AGENT_TRANS, (void **) &requestedTrans);
	    if (requestedTrans == XDPSNX_USE_BEST) {
		XDPSNXSetClientArg(XDPSNX_LAUNCHED_AGENT_TRANS,
			    (void *)XDPSNX_TRANS_UNIX);
		requestedTrans = XDPSNX_TRANS_UNIX;
	    }
	    /* cons-up an arg. to pass to Agent we are forking */
    
	    additionalArgs[1] = (char *) NULL;
	    additionalArgs[0] = transportArg;
	    XDPSGetNXArg(XDPSNX_LAUNCHED_AGENT_PORT, (void **) &requestedPort);
	    if (requestedPort == XDPSNX_USE_BEST) {
		requestedPort = XDPSNXRecommendPort(requestedTrans);
		if (requestedPort < 0) {
		DPSWarnProc(NULL, "Auto-launcher can't get a port.");
		return(NULL);
		}
	    }
	    (void) sprintf(transportArg, "%s/%d",
			    (requestedTrans == XDPSNX_TRANS_DECNET ?
			    "dec" : "tcp"),
			    requestedPort);
	    args = additionalArgs;
	    /* ASSERT: first argument in additionalArgs must be
		transport/port, unless agent changes to take this
		anywhere */
	    if (StartXDPSNX(args) != Success) {
	        char tb[256], *fs, **as;
                (void) XDPSGetNXArg(XDPSNX_EXEC_FILE, (void **) &fs);
                (void) XDPSGetNXArg(XDPSNX_EXEC_ARGS, (void **) &as);
		(void) sprintf(tb, "FAILED to auto-launch:\n    %s", fs);
		if (as != NULL)
		    for (; *as != NULL; as++) {
		        if ((int) (strlen(*as) + 1 + (i = strlen(tb))) > 256-1) {
			    if (i > 256-1-4)
			        strcpy(&(tb[256-1-1-4]), " ...");
			    else
			        strcat(tb, " ...");
			    break;
			}
			(void) strcat(tb, " ");
		        (void) strcat(tb, *as);
		    }
	        DPSWarnProc(NULL, tb);
		return(NULL);
	    } else {
		(void) sprintf(display_name, "%s%s%d", hostname,
				(requestedTrans == XDPSNX_TRANS_DECNET ?
				"::" : ":"),
				requestedPort);
	    }
	    } else {		/* autoLaunch != True */
	    return(NULL);
	    }
	  }
          break;
	case findnx_found: {	/* XDPSNXFindNX() == Success */
	    (void) sprintf(display_name, "%s%s%d",
			 (transport == XDPSNX_TRANS_UNIX ?
			  "unix" : agentHost),
			 (transport == XDPSNX_TRANS_DECNET ? "::" : ":"),
			 port);
	    /* Free agentHost later */
	  }
          break;
	case findnx_error:
          return(NULL);
	  break;
	default:
          DPSFatalProc(NULL, "Illegal value returned by XDPSNXFindNX");
	  break;
      }


/*
 * Attempt to allocate a display structure. Return NULL if allocation fails.
 */
	if ((agent = (Display *)Xcalloc(1, sizeof(Display))) == NULL) {
		return(NULL);
	}

/*
 * Call the Connect routine to get the network socket. If -1 is returned, the
 * connection failed. The connect routine will set fullname to point to the
 * expanded name.
 */

	if ((agent->fd = DPSCAPConnect(display_name, &fullname, &idisplay,
					 &conn_family,
					 &server_addrlen, &server_addr)) < 0) {
		Xfree ((char *) agent);
		return(NULL);
	}
#ifdef XXX
/*
 * Look up the authorization protocol name and data if necessary.
 */
	if (xauth_name && xauth_data) {
	    conn_auth_namelen = xauth_namelen;
	    conn_auth_name = xauth_name;
	    conn_auth_datalen = xauth_datalen;
	    conn_auth_data = xauth_data;
	} else {
	    char dpynumbuf[40];		/* big enough to hold 2^64 and more */
	    (void) sprintf (dpynumbuf, "%d", idisplay);

	    authptr = XauGetAuthByAddr ((unsigned short) conn_family,
					(unsigned short) server_addrlen,
					server_addr,
					(unsigned short) strlen (dpynumbuf),
					dpynumbuf,
					(unsigned short) xauth_namelen,
					xauth_name);
	    if (authptr) {
		conn_auth_namelen = authptr->name_length;
		conn_auth_name = (char *)authptr->name;
		conn_auth_datalen = authptr->data_length;
		conn_auth_data = (char *)authptr->data;
	    } else {
		conn_auth_namelen = 0;
		conn_auth_name = NULL;
		conn_auth_datalen = 0;
		conn_auth_data = NULL;
	    }
	}
#ifdef HASDES
	/*
	 * build XDM-AUTHORIZATION-1 data
	 */
	if (conn_auth_namelen == 19 &&
	    !strncmp (conn_auth_name, "XDM-AUTHORIZATION-1", 19))
	{
	    static char    encrypted_data[192/8];
	    int	    i, j;
	    struct sockaddr_in	in_addr;
	    int	    addrlen;
	    long    now;

	    j = 0;
	    for (i = 0; i < 8; i++)
	    {
		encrypted_data[j] = conn_auth_data[i];
		j++;
	    }
	    addrlen = sizeof (in_addr);
	    getsockname (dpy->fd, (struct sockaddr *) &in_addr, &addrlen);
	    if (in_addr.sin_family == 2)
	    {
		encrypted_data[j] = in_addr.sin_addr.s_net; j++;
		encrypted_data[j] = in_addr.sin_addr.s_host; j++;
		encrypted_data[j] = in_addr.sin_addr.s_lh; j++;
		encrypted_data[j] = in_addr.sin_addr.s_impno; j++;
		encrypted_data[j] = (in_addr.sin_port >> 8) & 0xff; j++;
		encrypted_data[j] = (in_addr.sin_port) & 0xff; j++;
	    }
	    else
	    {
		encrypted_data[j] = 0xff; j++;
		encrypted_data[j] = 0xff; j++;
		encrypted_data[j] = 0xff; j++;
		encrypted_data[j] = 0xff; j++;
		i = getpid ();
		encrypted_data[j] = (i >> 8) & 0xff; j++;
		encrypted_data[j] = (i) & 0xff; j++;
	    }
	    time (&now);
	    for (i = 3; i >= 0; i--)
	    {
		encrypted_data[j] = (now >> (i * 8)) & 0xff;
		j++;
	    }
	    XdmcpEncrypt (encrypted_data, conn_auth_data + 8,
			  encrypted_data, 192/8);
	    conn_auth_data = encrypted_data;
	    conn_auth_datalen = 192 / 8;
	}
#endif /* HASDES */
	if (server_addr) (void) Xfree (server_addr);

#endif /* XXX */

/*
 * We succeeded at authorization, so let us move the data into
 * the display structure.
 */
	agent->lock_meaning	= NoSymbol;
#ifdef XXX
	/* this field is not present in post X11R5 */	
	agent->current		= None;
#endif	
	agent->event_vec[X_Error] = N_XUnknownWireEvent;
	agent->event_vec[X_Reply] = N_XUnknownWireEvent;
	agent->wire_vec[X_Error]  = N_XUnknownNativeEvent;
	agent->wire_vec[X_Reply]  = N_XUnknownNativeEvent;
	for (i = KeyPress; i < 128; i++) {
	    agent->event_vec[i] = N_XUnknownWireEvent;
	    agent->wire_vec[i] 	= N_XUnknownNativeEvent;
	}
	agent->cursor_font	= None;
	agent->last_req = (char *)&_dummy_request;

	/* Salt away the host:display string for later use.
	   Storage owned by agent, Xmalloc'd by connection
	   call above */
	agent->display_name = fullname;
 
	/* Set up the output buffers. */
	if ((agent->bufptr = agent->buffer = Xmalloc(BUFSIZE)) == NULL) {
	        OutOfMemory (dpy);
		return(NULL);
	}
	agent->bufmax = agent->buffer + BUFSIZE;


    /* Create extension data */

    my = DPSCAPCreate(dpy, agent);
    if (my == (DPSCAPData)NULL)
        {
        OutOfMemory(agent);
        return(NULL);
        }
    ext = (XExtData *)Xcalloc(1, sizeof(XExtData));
    ext->private_data = (char *)my;
    
    /* Parse names to get true display name */
    if (agentHost && strcmp(hostname, agentHost))
        {
	register char *s, *p;
	char *dxname;
	char nametmp[MAXHOSTNAMELEN];
	/* Agent is not on the same host as client, so fix
	   up the stupid abbreviations used for the display,
	   and whoever came up with the syntax should be shot. */
	dxname = DisplayString(dpy);
	for (s = dxname, p = nametmp; *s; ++s)
	    if (*s == ':')
	       break;
	    else
	       *p++ = *s;
	*p = '\0';
	if (nametmp[0] == '\0' 
	  || !strcmp(nametmp, "unix") 
	  || !strcmp(nametmp, "localhost"))
	    {
	    strcpy(trueDisplayName, hostname);
	    if (*s)
	        strcat(trueDisplayName, s);
	    else
	        strcat(trueDisplayName, ":0.0");
	    }
	else
	    strcpy(trueDisplayName, dxname);
	}
    else
        strcpy(trueDisplayName, DisplayString(dpy));
    if (agentHost)
        Xfree(agentHost);
    return(ext);
}


/* OutOfMemory is called if malloc fails.  XOpenDisplay returns NULL
   after this returns. */

static void OutOfMemory (Display *dpy)
{
    DPSCAPCloseAgent(dpy);
}

#ifdef NEEDFORNX
/* XFreeDisplayStructure frees all the storage associated with a 
 * Display.  It is used by XOpenDisplay if it runs out of memory,
 * and also by XCloseDisplay.   It needs to check whether all pointers
 * are non-NULL before dereferencing them, since it may be called
 * by XOpenDisplay before the Display structure is fully formed.
 * XOpenDisplay must be sure to initialize all the pointers to NULL
 * before the first possible call on this.
 */

static void
_XFreeDisplayStructure(register Display *dpy)
{
	if (dpy->screens) {
	    register int i;

            for (i = 0; i < dpy->nscreens; i++) {
		Screen *sp = &dpy->screens[i];

		if (sp->depths) {
		   register int j;

		   for (j = 0; j < sp->ndepths; j++) {
			Depth *dp = &sp->depths[j];

			if (dp->visuals) {
			   register int k;

			   for (k = 0; k < dp->nvisuals; k++)
			     _XFreeExtData (dp->visuals[k].ext_data);
			   Xfree ((char *) dp->visuals);
			   }
			}

		   Xfree ((char *) sp->depths);
		   }

		_XFreeExtData (sp->ext_data);
		}

	    Xfree ((char *)dpy->screens);
	    }
	
	if (dpy->pixmap_format) {
	    register int i;

	    for (i = 0; i < dpy->nformats; i++)
	      _XFreeExtData (dpy->pixmap_format[i].ext_data);
            Xfree ((char *)dpy->pixmap_format);
	    }

	if (dpy->display_name)
	   Xfree (dpy->display_name);
	if (dpy->vendor)
	   Xfree (dpy->vendor);

        if (dpy->buffer)
	   Xfree (dpy->buffer);
	if (dpy->atoms)
	   Xfree ((char *) dpy->atoms);
	if (dpy->keysyms)
	   Xfree ((char *) dpy->keysyms);
	if (dpy->modifiermap)
	   XFreeModifiermap(dpy->modifiermap);
	if (dpy->xdefaults)
	   Xfree (dpy->xdefaults);
	if (dpy->key_bindings)
	   _XFreeKeyBindings(dpy);

	while (dpy->ext_procs) {
	    _XExtension *ext = dpy->ext_procs;
	    dpy->ext_procs = ext->next;
	    if (ext->name)
		Xfree (ext->name);
	    Xfree ((char *)ext);
	}

	_XFreeExtData (dpy->ext_data);
        
	Xfree ((char *)dpy);
}
#endif /* NEEDFORNX */



void
DPSCAPCloseAgent(Display *agent)
{
    if (!agent) return;
    N_XDisconnectDisplay(agent->fd);
    if (agent->display_name)
        Xfree(agent->display_name);
    if (agent->buffer)
        Xfree(agent->buffer);
    Xfree((char *)agent);
}
