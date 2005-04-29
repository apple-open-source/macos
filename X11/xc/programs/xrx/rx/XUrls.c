/* $Xorg: XUrls.c,v 1.4 2001/02/09 02:05:58 xorgcvs Exp $ */
/*

Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABIL-
ITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization from
The Open Group.

*/
/* $XFree86: xc/programs/xrx/rx/XUrls.c,v 1.12 2003/07/20 16:12:20 tsi Exp $ */

#include "RxI.h"
#include "XUrls.h"
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <limits.h>		/* for MAXHOSTNAMELEN */
#include <errno.h>

/* and in case we didn't get it from the headers above */
#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN 256
#endif

/*
 * Return a good hostname, even on multi-homed hosts, e.g. machines
 * connected to the internet via PPP which also have a local network
 * are multi-homed.
 *
 * This might still fail to provide the right name if netscape is
 * running on a different host than it is being displayed on. E.g,
 *
 *   netscapehost <--ethernet--> display host <--ppp--//--> webserver
 *
 * The display host is multi-homed and has the nodename myhost.mynet
 * as far as netscape and the netscape host are concerned, but has
 * the name pppX.myispnet to the outside world. When this happens the
 * user is going to have to set the XREALDISPLAY environment variable
 * when they run netscape like this.
 */
static char*
MyBestHostname (
  char* myname, 
  int myname_len, 
  char* display_name, 
  char* dest_url)
{
  struct utsname host;

  *myname = '\0';

  /* if of the form ":0.0" get the real hostname */
  if (display_name[0] == ':') {

/* for some reason this doesn't work on Solaris 2.x */
#if !(defined(sun) && defined(SVR4))
    struct sockaddr_in local, remote;
    struct hostent* hp;
    int s, rv, namelen;
    char dest_hostname[MAXHOSTNAMELEN + 1];

    ParseHostname (dest_url, dest_hostname, sizeof dest_hostname);

    hp = gethostbyname (dest_hostname);

    if (hp) {
      memcpy (&remote.sin_addr, hp->h_addr_list[0], sizeof remote.sin_addr);
      remote.sin_port = htons(60000);
      remote.sin_family = AF_INET;
#ifdef BSD44SOCKETS
      remote.sin_len = sizeof remote;
#endif

      local.sin_addr.s_addr = htonl(INADDR_ANY);
      local.sin_port = htons(60000);
      local.sin_family = AF_INET;
#ifdef BSD44SOCKETS
      local.sin_len = sizeof remote;
#endif

      s = socket(PF_INET, SOCK_DGRAM, 0);
      if (s != -1) {
	do {
	  rv = bind (s, (struct sockaddr*) &local, sizeof local);
	  local.sin_port = htons (ntohs (local.sin_port) + 1);
	} while (rv == -1 && errno == EADDRINUSE);

	if (rv != -1) {
	  do {
	    rv = connect (s, (struct sockaddr*) &remote, sizeof remote);
	    remote.sin_port = htons (ntohs (remote.sin_port) + 1);
	  } while (rv == -1 && errno == EADDRINUSE);

	  if (rv != -1) {
	    namelen = sizeof local;
	    rv = getsockname (s, (struct sockaddr*) &local, (void *)&namelen);

	    if (rv != -1) {
	      hp = gethostbyaddr ((char*) &local.sin_addr.s_addr, 
				  sizeof local.sin_addr.s_addr, 
				  AF_INET);
	      if (hp) {
		strncpy (myname, hp->h_name, myname_len);
		myname[MAXHOSTNAMELEN] = '\0';
		close (s);
		return display_name;
	      }
	    }
	  }
	}
	close (s);
      }
    }
#endif 
  /* none of the above worked, punt */

    uname(&host);
    strncpy (myname, host.nodename, myname_len);
    myname[MAXHOSTNAMELEN] = '\0';
  } else {	/* otherwise believe the display_name */
    char *ptr;

    ptr = strrchr(display_name, ':');
    if (ptr == NULL) {
      /* if there's no ":0" in the name, just copy it */
      strncpy(myname, display_name, myname_len);
      myname[MAXHOSTNAMELEN] = '\0';
    } else {
      /* otherwise copy everthing up to the ":0" */
      strncpy(myname, display_name, ptr - display_name);
      myname[ptr - display_name] = '\0';
      return ptr;
    }
  }
  return display_name;
}


/* return the Url of the user's X display
 * this must be of the form:
 *   x11: [protocol/] [hostname] : [:] displaynumber [.screennumber] \
 *	[ ; auth= auth_data ]
 */
char *
GetXUrl(char *display_name, char *auth, char* dest_url)
{
    char *dpy_name, *proto = NULL;
    char *url, *ptr;
    char *name;
    struct hostent *host;
    int len, proto_len, dpy_len, name_len, auth_len;
    char hostname[MAXHOSTNAMELEN + 1];

    len = 5;			/* this is for "x11:" */

    /* if of the form "x11:display" change it to "display" */
    if (strncmp(display_name, "x11:", 4) == 0)
	dpy_name = display_name + 4;
    else
	dpy_name = display_name;

    /* if protocol is specified store it and remove it from display name  */
    ptr = strchr(dpy_name, '/');
    if (ptr != NULL) {
	proto = dpy_name;
	proto_len = ptr - dpy_name;
	dpy_name = ptr + 1;
	/* if local forget it */
	if (strncmp(proto, "local", proto_len) == 0)
	    proto_len = 0;
    } else
	proto_len = 0;

    /* if of the form "unix:0.0" change it to ":0.0" */
    if (strncmp(dpy_name, "unix", 4) == 0)
	dpy_name += 4;

    dpy_name = MyBestHostname (hostname, MAXHOSTNAMELEN, dpy_name, dest_url);
    host = gethostbyname(hostname);
    name = host->h_name;
    name_len = strlen(name);

    dpy_len = dpy_name == NULL ? 0 : strlen(dpy_name);
    auth_len = auth == NULL ? 0 : 6 + strlen(auth); /* 6 for ";auth=" */
    len += proto_len + 1 + name_len + dpy_len + auth_len;

    url = ptr = (char *)Malloc(len);
    if (url == NULL)
	return NULL;

    strcpy(ptr, "x11:");
    ptr += 4;
    if (proto_len != 0) {
	strncpy(ptr, proto, proto_len + 1);
	ptr += proto_len + 1;
    }
    if (name_len != 0) {
	strcpy(ptr, name);
	ptr += name_len;
    }
    if (dpy_len != 0) {
	strcpy(ptr, dpy_name);
	ptr += dpy_len;
    }
    if (auth_len != 0)
	sprintf(ptr, ";auth=%s", auth);
    else
	*ptr = '\0';

    return url;
}


/* return the Url of the user's XPrint server
 * this must be of the form:
 *   xprint: [printername@] [protocol/] [hostname] : [:] displaynumber \
 *	[ ; auth= auth_data ]
 */
char *
GetXPrintUrl(char *display_name, char *printer, char *auth, char* dest_url)
{
    char *dpy_name, *proto = NULL;
    char *url, *ptr;
    char *name;
    struct hostent *host;
    int len, proto_len, dpy_len, name_len, auth_len, printer_len;
    char hostname[MAXHOSTNAMELEN + 1];

    len = 7;			/* this is for "xprint:" */

    /* if of the form "xprint:display" change it to "display" */
    if (strncmp(display_name, "xprint:", 7) == 0)
	dpy_name = display_name + 7;
    else
	dpy_name = display_name;

    /* if protocol is specified store it and remove it from display name  */
    ptr = strchr(dpy_name, '/');
    if (ptr != NULL) {
	proto = dpy_name;
	proto_len = ptr - dpy_name;
	dpy_name = ptr + 1;
	/* if local forget it */
	if (strncmp(proto, "local", proto_len) == 0)
	    proto_len = 0;
    } else
	proto_len = 0;

    /* if of the form "unix:0.0" change it to ":0.0" */
    if (strncmp(dpy_name, "unix", 4) == 0)
	dpy_name += 4;

    dpy_name = MyBestHostname (hostname, MAXHOSTNAMELEN, dpy_name, dest_url);
    host = gethostbyname(hostname);
    name = host->h_name;

    /* if a screen number is specified strip it - not valid for printing */
    ptr = strchr(dpy_name, '.');
    if (ptr != NULL)
	dpy_len = ptr - dpy_name;
    else
	dpy_len = strlen(dpy_name);

    name_len = strlen(name);
    printer_len = printer ? strlen(printer) : 0;
    auth_len = auth == NULL ? 0 : 6 + strlen(auth); /* 6 for ";auth=" */
    len += printer_len + 1 + proto_len + 1 + name_len + dpy_len + auth_len;

    url = ptr = (char *)Malloc(len);
    if (url == NULL)
	return NULL;

    strcpy(ptr, "xprint:");
    ptr += 7;
    if (printer_len != 0) {
	strcpy(ptr, printer);
	ptr += printer_len;
	*ptr++ = '@';
    }
    if (proto_len != 0) {
	strncpy(ptr, proto, proto_len + 1);
	ptr += proto_len + 1;
    }
    if (name_len != 0) {
	strcpy(ptr, name);
	ptr += name_len;
    }
    if (dpy_len != 0) {
	strncpy(ptr, dpy_name, dpy_len);
	ptr += dpy_len;
    }
    if (auth_len != 0)
	sprintf(ptr, ";auth=%s", auth);
    else
	*ptr = '\0';

    return url;
}

/*
 * Extract the hostname of a given url. The hostname is copied to the given
 * buffer if it is big enough and its length is returned.
 * This relies strongly on the following expected syntax:
 *        scheme : [//] hostname [: port] [/path]
 */
int
ParseHostname(char *url, char *buf, int buflen)
{
    char *ptr, *begin;

    if (url == NULL)
	return 0;

    /* skip scheme part */
    ptr = strchr(url, ':');
    if (ptr != NULL)
	ptr++;
    else
	ptr = url;
    /* skip possible "//" */
    while (*ptr && *ptr == '/')
	ptr++;
    begin = ptr;
    /* Check for RFC 2732 bracketed IPv6 numeric address */
    if (*ptr == '[') {
	begin++;
	while (*ptr && (*ptr != ']')) {
	    ptr++;
	}
    } else {
	/* look for possible port specification */
	ptr = strchr(begin, ':');
	if (ptr == NULL) {
	    /* look for possible path */
	    ptr = strchr(begin, '/');
	    if (ptr == NULL)
		ptr += strlen(begin);
	}
    }
    if (ptr - begin < buflen) {
	strncpy(buf, begin, ptr - begin);
	buf[ptr - begin] = '\0';
	return ptr - begin;
    } else
	return 0;
}
