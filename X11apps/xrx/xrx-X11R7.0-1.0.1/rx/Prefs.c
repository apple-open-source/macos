/* $Xorg: Prefs.c,v 1.5 2001/02/09 02:05:58 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xrx/rx/Prefs.c,v 1.5 2001/01/17 23:46:25 dawes Exp $ */

#include "Prefs.h"
#include <ctype.h>
#include "RxI.h"		/* for Malloc & Free */
#include <X11/StringDefs.h>
#ifndef Lynx
#include <sys/socket.h>
#else
#include <socket.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef struct {
    Boolean has_fwp;
    String internal_webservers;	/* servers for which we won't use a fwp */
    String trusted_webservers;
    String fast_webservers;	/* servers for which we won't use lbx */
} Prefs;

#define offset(field) XtOffsetOf(Prefs, field)

static XtResource resources[] = {
    { "XrxHasFirewallProxy", "xrxHasFirewallProxy",
      XtRBoolean, sizeof(Boolean),
      offset(has_fwp), XtRImmediate, (caddr_t) False },
    { "XrxInternalWebServers", "xrxInternalWebServers",
      XtRString, sizeof(String),
      offset(internal_webservers), XtRImmediate, (caddr_t) NULL },
    { "XrxTrustedWebServers", "xrxTrustedWebServers",
      XtRString, sizeof(String),
      offset(trusted_webservers), XtRImmediate, (caddr_t) NULL },
    { "XrxFastWebServers", "xrxFastWebServers", XtRString, sizeof(String),
      offset(fast_webservers), XtRImmediate, (caddr_t) NULL }
};


/*
 * Return the next available element in a list of AddressFilters, making the
 * list bigger when necessary.
 */
/* how much bigger we make the list every time we reach the limit */
#define SIZEINC 8

static AddressFilter *
NextAFListElem(AddressFilter **list, int *count)
{
    AddressFilter *l;
    int n;

    l = *list;
    n = *count;

    /* first make sure the list is big enough */
    if (n == 0) {
	l = (AddressFilter *)Malloc(sizeof(AddressFilter) * SIZEINC);
	if (l == NULL)
	    return NULL;
	*list = l;
    } else if (n % SIZEINC == 0) { /* we need to enlarge the list */
	l = (AddressFilter *)Realloc(l, sizeof(AddressFilter) * n,
				     sizeof(AddressFilter) * (n + SIZEINC));
	if (l == NULL)
	    return NULL;
	*list = l;
    }
    /* then just return the first available element */
    *count = n + 1;
    return (*list) + n;
}

/*
 * find the end of next element of a comma separated string,
 * returning the pointer to the trailing string or NULL if there is none
 */
static char *
NextListElem(char *ptr, char **end_ret)
{
    char *end = strchr(ptr, ',');
    if (end != NULL) {
	/* skip comma and possible following space */
	ptr = end + 1;
	while (*ptr && isspace(*ptr))
	    ptr++;
    } else {
	end = ptr + strlen(ptr);
	ptr = NULL;
    }
    *end_ret = end;
    return ptr;
}


/*
 * retreive the two parts of mask/value from the given string, specified by
 * its beginning and end, and copy them into the given buffers
 */
static int
ParseListElem(char *bos, char *eos,
	      char *buf1, int len1, char *buf2, int len2)
{
    char *sep = strchr(bos, '/');
    if (sep != NULL) {
	int len = sep - bos;
	if (len < len1) {
	    strncpy(buf1, bos, len);
	    buf1[len] = '\0';
	    /* now deal with the second part */
	    bos = sep + 1;
	    len = eos - bos;
	    if (len < len2) {
		strncpy(buf2, bos, len);
		buf2[len] = '\0';
		return 1;
	    }
	}
    }
    return 0;			/* failed */
}


/*
 * parse a comma separated list of AddressFilters: mask/value
 */
static void
ParseList(String string, AddressFilter **list_return, int *count_return)
{
    char *ptr, *boe, *eoe;
#define BUFLEN 32
    char mask[BUFLEN], value[BUFLEN];
    AddressFilter *elem;

    *list_return = NULL;
    *count_return = 0;
    if (string == NULL || *string == '\0')
	return;

    ptr = string;
    do {
	boe = ptr;
	ptr = NextListElem(ptr, &eoe);
	if (boe && eoe) {
	    elem = NULL;
	    if (ParseListElem(boe, eoe, mask, BUFLEN, value, BUFLEN) != 0) {
		unsigned long imask = inet_addr(mask);
		unsigned long ivalue = inet_addr(value);
		if (((long) imask) != -1 && ((long) ivalue) != -1) {
		    elem = NextAFListElem(list_return, count_return);
		    elem->mask = imask; 
		    elem->value = ivalue;
		}
	    }
	    if (elem == NULL) {
		/* copy whathever we can in one of our bufs to print it out */
#define MYMIN(a,b) ((a) > (b) ? (b) : (a))
		int len = MYMIN(eoe - boe, BUFLEN - 1);
		strncpy(mask, boe, len);
		mask[len] = '\0';
		fprintf(stderr,
			"Could not convert \"%s\" into a pair mask/value\n",
			mask);
	    }
	}
    } while (ptr && *ptr);
}

void
GetPreferences(Widget widget, Preferences *preferences)
{
    Prefs prefs;
    XtGetApplicationResources(widget, &prefs,
			      resources, XtNumber(resources),
			      NULL, 0);

    preferences->has_fwp = prefs.has_fwp;
    ParseList(prefs.internal_webservers,
	      &preferences->internal_webservers,
	      &preferences->internal_webservers_count);
    ParseList(prefs.trusted_webservers,
	      &preferences->trusted_webservers,
	      &preferences->trusted_webservers_count);
    ParseList(prefs.fast_webservers,
	      &preferences->fast_webservers,
	      &preferences->fast_webservers_count);
}

void
FreePreferences(Preferences *preferences)
{
    if (preferences->internal_webservers)
	Free(preferences->internal_webservers);
    if (preferences->trusted_webservers)
	Free(preferences->trusted_webservers);
    if (preferences->fast_webservers)
	Free(preferences->fast_webservers);
}

static Boolean
FilterHost(char *hostname, AddressFilter *filters, int count)
{
    struct hostent *host;
    unsigned int addr;
    int i;

    if (count == 0 || filters == NULL)
	return False;

    /* first find the host address number */
    host = gethostbyname(hostname);
    if (host == NULL || host->h_addrtype != AF_INET)
	/* host or address type unknown */
	return False;

    addr = ((struct in_addr*) host->h_addr_list[0])->s_addr;
    
    for (i = 0; i < count; i++, filters++)
	if ((addr & filters->mask) == (filters->value & filters->mask))
	    return True;

    return False;
}

void
ComputePreferences(Preferences *prefs, char *webserver,
	      Boolean *trusted_ret, Boolean *use_fwp_ret, Boolean *use_lbx_ret)
{
    if (webserver != NULL) {
	if (prefs->has_fwp == True) {
	    *use_fwp_ret =
		FilterHost(webserver,
			   prefs->internal_webservers,
			   prefs->internal_webservers_count) ? False : True;
	} else
	    *use_fwp_ret = False;

	*trusted_ret = FilterHost(webserver,
				  prefs->trusted_webservers,
				  prefs->trusted_webservers_count);
	*use_lbx_ret = FilterHost(webserver,
				  prefs->fast_webservers,
				  prefs->fast_webservers_count) ? False : True;
    } else {
	/* can't do much without webserver name */
	*use_fwp_ret = prefs->has_fwp;
	*trusted_ret = False;
	*use_lbx_ret = True;
    }
}
