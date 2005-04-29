/*
 * $Xorg: access.c,v 1.5 2001/02/09 02:05:40 xorgcvs Exp $
 *
Copyright 1990, 1998  The Open Group
Copyright 2002 Sun Microsystems, Inc.  All rights reserved.

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the copyright holder.
 *
 * Author:  Keith Packard, MIT X Consortium
 */

/* $XFree86: xc/programs/xdm/access.c,v 3.14 2003/11/23 22:02:07 herrb Exp $ */

/*
 * Access control for XDMCP - keep a database of allowable display addresses
 * and (potentially) a list of hosts to send ForwardQuery packets to
 */

# include   "dm.h"
# include   "dm_error.h"

#ifdef XDMCP

# include   <X11/Xos.h>
# include   <X11/Xdmcp.h>
# include   <X11/X.h>
# include   <stdio.h>
# include   <ctype.h>

# include   "dm_socket.h"

# include   <netdb.h>

# if defined(IPv6) && defined(AF_INET6)
#  include        <arpa/inet.h>
# endif

#define ALIAS_CHARACTER	    '%'
#define NEGATE_CHARACTER    '!'
#define CHOOSER_STRING	    "CHOOSER"
#define BROADCAST_STRING    "BROADCAST"
#define NOBROADCAST_STRING  "NOBROADCAST"
#define LISTEN_STRING	    "LISTEN"
#define WILDCARD_STRING	    "*"

#define HOST_ALIAS	    0
#define HOST_ADDRESS	    1
#define HOST_BROADCAST	    2
#define HOST_CHOOSER	    3
#define HOST_NOBROADCAST    4
#define HOST_ANYADDR	    5

typedef struct _hostEntry {
    struct _hostEntry	*next;
    int	    type;
    union _hostOrAlias {
	char	*aliasName;
	ARRAY8	hostAddress;
    } entry;
    int			hopCount;
} HostEntry;

#define DISPLAY_ALIAS	    0
#define DISPLAY_PATTERN	    1
#define DISPLAY_ADDRESS	    2
#define DISPLAY_LISTEN	    3

typedef struct _displayEntry {
    struct _displayEntry    *next;
    int			    type;
    int			    notAllowed;
    int			    notBroadcast;
    int			    chooser;
    union _displayType {
	char		    *aliasName;
	char		    *displayPattern;
	struct _display {
	    ARRAY8	    clientAddress;
	    CARD16	    connectionType;
	} displayAddress;
    } entry;
    HostEntry		    *hosts;
} DisplayEntry;

static DisplayEntry	*database;

static ARRAY8		localAddress;

ARRAY8Ptr
getLocalAddress (void)
{
    static int	haveLocalAddress;
    
    if (!haveLocalAddress)
    {
#if defined(IPv6) && defined(AF_INET6)
	struct addrinfo *ai;

	if (getaddrinfo(localHostname(), NULL, NULL, &ai) != 0) {
	    XdmcpAllocARRAY8 (&localAddress, 4);
	    localAddress.data[0] = 127;
	    localAddress.data[1] = 0;
	    localAddress.data[2] = 0;
	    localAddress.data[3] = 1;
	} else {
	    if (ai->ai_addr->sa_family == AF_INET) {
		XdmcpAllocARRAY8 (&localAddress, sizeof(struct in_addr));
		memcpy(localAddress.data, 
		  &((struct sockaddr_in *)ai->ai_addr)->sin_addr,
		  sizeof(struct in_addr));
	    } else if (ai->ai_addr->sa_family == AF_INET6) {
		XdmcpAllocARRAY8 (&localAddress, sizeof(struct in6_addr));
		memcpy(localAddress.data, 
		  &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr,
		  sizeof(struct in6_addr));
	    }	      
	    freeaddrinfo(ai);
	}
#else
	struct hostent	*hostent;

	hostent = gethostbyname (localHostname());
	XdmcpAllocARRAY8 (&localAddress, hostent->h_length);
	memmove( localAddress.data, hostent->h_addr, hostent->h_length);
#endif

    }
    return &localAddress;
}

static void
FreeHostEntry (HostEntry *h)
{
    switch (h->type) {
    case HOST_ALIAS:
	free (h->entry.aliasName);
	break;
    case HOST_ADDRESS:
	XdmcpDisposeARRAY8 (&h->entry.hostAddress);
	break;
    case HOST_CHOOSER:
	break;
    }
    free ((char *) h);
}

static void
FreeDisplayEntry (DisplayEntry *d)
{
    HostEntry	*h, *next;
    switch (d->type) {
    case DISPLAY_ALIAS:
	free (d->entry.aliasName);
	break;
    case DISPLAY_PATTERN:
	free (d->entry.displayPattern);
	break;
    case DISPLAY_ADDRESS:
	XdmcpDisposeARRAY8 (&d->entry.displayAddress.clientAddress);
	break;
    case DISPLAY_LISTEN:
	/* do nothing - this case doesn't use the d->entry union */
	break;
    }
    for (h = d->hosts; h; h = next) {
	next = h->next;
	FreeHostEntry (h);
    }
    free ((char *) d);
}

static void
FreeAccessDatabase (void)
{
    DisplayEntry    *d, *next;

    for (d = database; d; d = next)
    {
	next = d->next;
	FreeDisplayEntry (d);
    }
    database = 0;
}

#define WORD_LEN    256
static char	wordBuffer[WORD_LEN];
static int	nextIsEOF;

static char *
ReadWord (FILE *file, int EOFatEOL)
{
    int	    c;
    char    *wordp;
    int	    quoted;

    wordp = wordBuffer;
    if (nextIsEOF)
    {
	nextIsEOF = FALSE;
	return NULL;
    }
    quoted = FALSE;
    for (;wordp - wordBuffer < sizeof(wordBuffer)-2;) {
	c = getc (file);
	switch (c) {
	case '#':
	    if (quoted)
	    {
		*wordp++ = c;
		break;
	    }
	    while ((c = getc (file)) != EOF && c != '\n')
		;
	case '\n':
	case EOF:
	    if (c == EOF || (EOFatEOL && !quoted))
	    {
		ungetc (c, file);
		if (wordp == wordBuffer)
		    return NULL;
		*wordp = '\0';
		nextIsEOF = TRUE;
		return wordBuffer;
	    }
	case ' ':
	case '\t':
	    if (wordp != wordBuffer)
	    {
		ungetc (c, file);
		*wordp = '\0';
		return wordBuffer;
	    }
	    break;
	case '\\':
	    if (!quoted)
	    {
		quoted = TRUE;
		continue;
	    }
	default:
	    if (wordp < &(wordBuffer[WORD_LEN]))
	      *wordp++ = c;
	    break;
	}
	quoted = FALSE;
    }
    return NULL;
}

static HostEntry *
ReadHostEntry (FILE *file)
{
    char	    *hostOrAlias;
    HostEntry	    *h;

tryagain:
    hostOrAlias = ReadWord (file, TRUE);
    if (!hostOrAlias)
	return NULL;
    h = (HostEntry *) malloc (sizeof (DisplayEntry));
    h->hopCount = 1;
    if (*hostOrAlias == ALIAS_CHARACTER)
    {
	h->type = HOST_ALIAS;
	h->entry.aliasName = malloc (strlen (hostOrAlias) + 1);
	if (!h->entry.aliasName) {
	    free ((char *) h);
	    return NULL;
	}
	strcpy (h->entry.aliasName, hostOrAlias);
    }
    else if (!strcmp (hostOrAlias, CHOOSER_STRING))
    {
	h->type = HOST_CHOOSER;
    }
    else if (!strcmp (hostOrAlias, BROADCAST_STRING))
    {
	h->type = HOST_BROADCAST;
    }
    else if (!strcmp (hostOrAlias, NOBROADCAST_STRING))
    {
	h->type = HOST_NOBROADCAST;
    }
    else if (!strcmp (hostOrAlias, WILDCARD_STRING))
    {
	h->type = HOST_ANYADDR;
	h->entry.hostAddress.length = 0;
    }
    else
    {
	void *addr=NULL;
	size_t addr_length=0;
#if defined(IPv6) && defined(AF_INET6)
	struct addrinfo *ai;
#else
	struct hostent  *hostent = gethostbyname (hostOrAlias);
#endif	
	char *hops = strrchr(hostOrAlias, '/');

	if (hops) {
	    *(hops++) = '\0';
	    h->hopCount = strtol(hops, NULL, 10);
	    if (h->hopCount < 1)
		h->hopCount = 1;
	}

#if defined(IPv6) && defined(AF_INET6)
	if (getaddrinfo(hostOrAlias, NULL, NULL, &ai) == 0) {
	    if (ai->ai_addr->sa_family == AF_INET) {
		addr = &((struct sockaddr_in *)ai->ai_addr)->sin_addr;
		addr_length = sizeof(struct in_addr);
	    } else if (ai->ai_addr->sa_family == AF_INET6) {
		addr = &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
		addr_length = sizeof(struct in6_addr);
	    }	      
	}
#else
	if (hostent) {
	    addr = hostent->h_addr;
	    addr_length = hostent->h_length;
	}
#endif
	h->type = HOST_ADDRESS;

	if (!addr)
	{
	    Debug ("No such host %s\n", hostOrAlias);
	    LogError ("Access file \"%s\", host \"%s\" not found\n", accessFile, hostOrAlias);
	    free ((char *) h);
	    goto tryagain;
	}
	if (!XdmcpAllocARRAY8 (&h->entry.hostAddress, addr_length))
	{
	    LogOutOfMem ("ReadHostEntry\n");
	    free ((char *) h);
	    return NULL;
	}
	memmove( h->entry.hostAddress.data, addr, addr_length);
#if defined(IPv6) && defined(AF_INET6)
	freeaddrinfo(ai);
#endif
    }
    return h;
}

static int
HasGlobCharacters (char *s)
{
    for (;;)
	switch (*s++) {
	case '?':
	case '*':
	    return 1;
	case '\0':
	    return 0;
	}
}

static DisplayEntry *
ReadDisplayEntry (FILE *file)
{
    char	    *displayOrAlias;
    DisplayEntry    *d;
    struct _display *display;
    HostEntry	    *h, **prev;
    
    displayOrAlias = ReadWord (file, FALSE);
    if (!displayOrAlias)
    	return NULL;
    d = (DisplayEntry *) malloc (sizeof (DisplayEntry));
    d->notAllowed = 0;
    d->notBroadcast = 0;
    d->chooser = 0;
    if (*displayOrAlias == ALIAS_CHARACTER)
    {
	d->type = DISPLAY_ALIAS;
	d->entry.aliasName = malloc (strlen (displayOrAlias) + 1);
	if (!d->entry.aliasName)
	{
	    free ((char *) d);
	    return NULL;
	}
	strcpy (d->entry.aliasName, displayOrAlias);
    }
    else if (!strcmp(displayOrAlias, LISTEN_STRING)) 
    {
	d->type = DISPLAY_LISTEN;
    }
    else
    {
	if (*displayOrAlias == NEGATE_CHARACTER)
	{
	    d->notAllowed = 1;
	    ++displayOrAlias;
	}
    	if (HasGlobCharacters (displayOrAlias))
    	{
	    d->type = DISPLAY_PATTERN;
	    d->entry.displayPattern = malloc (strlen (displayOrAlias) + 1);
	    if (!d->entry.displayPattern)
	    {
	    	free ((char *) d);
	    	return NULL;
	    }
	    strcpy (d->entry.displayPattern, displayOrAlias);
    	}
    	else
    	{
	    void *addr = NULL;
	    size_t addr_length = 0;
	    int addrtype = 0;

#if defined(IPv6) && defined(AF_INET6)
	    struct addrinfo *ai;

	    if (getaddrinfo(displayOrAlias, NULL, NULL, &ai) == 0) {
		addrtype = ai->ai_addr->sa_family;
		if (addrtype == AF_INET) {
		    addr = &((struct sockaddr_in *)ai->ai_addr)->sin_addr;
		    addr_length = sizeof(struct in_addr);
		} else if (addrtype == AF_INET6) {
		    addr = &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
		    addr_length = sizeof(struct in6_addr);
		}	      
	    }
#else
	    struct hostent  *hostent;

	    if ((hostent = gethostbyname (displayOrAlias)) != NULL)
	    {
		Debug("ReadDisplayEntry: %s\n", displayOrAlias);
		addr = hostent->h_addr;
		addrtype = hostent->h_addrtype;
		addr_length = hostent->h_length;
	    }
#endif
	    if (addr == NULL)
	    {
		LogError ("Access file %s, display %s unknown\n", accessFile, displayOrAlias);
		free ((char *) d);
		return NULL;
	    }
	    d->type = DISPLAY_ADDRESS;
	    display = &d->entry.displayAddress;
	    if (!XdmcpAllocARRAY8 (&display->clientAddress, addr_length))
	    {
	    	free ((char *) d);
	    	return NULL;
	    }
	    memmove( display->clientAddress.data, addr, addr_length);
	    switch (addrtype)
	    {
#ifdef AF_UNIX
	    case AF_UNIX:
	    	display->connectionType = FamilyLocal;
	    	break;
#endif
#ifdef AF_INET
	    case AF_INET:
	    	display->connectionType = FamilyInternet;
	    	break;
#endif
#if defined(IPv6) && defined(AF_INET6)
	    case AF_INET6:
	    	display->connectionType = FamilyInternet6;
	    	break;
#endif
#ifdef AF_DECnet
	    case AF_DECnet:
	    	display->connectionType = FamilyDECnet;
	    	break;
#endif
	    default:
	    	display->connectionType = FamilyLocal;
	    	break;
	    }
    	}
    }
    prev = &d->hosts;
    while ((h = ReadHostEntry (file)))
    {
	if (h->type == HOST_CHOOSER)
	{
	    FreeHostEntry (h);
	    d->chooser = 1;
	} else if (h->type == HOST_NOBROADCAST) {
	    FreeHostEntry (h);
	    d->notBroadcast = 1;
	} else if (h->type == HOST_ANYADDR) {
	    if (d->type == DISPLAY_LISTEN) {
		*prev = h;
		prev = &h->next;
	    } else {
		Debug("Wildcard host specified in Xaccess for type other than LISTEN -- ignoring\n");
		FreeHostEntry (h);
	    }
	} else {
	    *prev = h;
	    prev = &h->next;
	}
    }
    *prev = NULL;
    return d;
}

static void
ReadAccessDatabase (FILE *file)
{
    DisplayEntry    *d, **prev;

    prev = &database;
    while ((d = ReadDisplayEntry (file)))
    {
	*prev = d;
	prev = &d->next;
    }
    *prev = NULL;
}

int
ScanAccessDatabase (void)
{
    FILE	*datafile;

    FreeAccessDatabase ();
    if (*accessFile)
    {
    	datafile = fopen (accessFile, "r");
    	if (!datafile)
	{
	    LogError ("Cannot open access control file %s, no XDMCP reqeusts will be granted\n", accessFile);
	    return 0;
	}
	ReadAccessDatabase (datafile);
	fclose (datafile);
    }
    return 1;
}

/*
 * calls the given function for each valid indirect entry.  Returns TRUE if
 * the local host exists on any of the lists, else FALSE
 */

#define MAX_DEPTH   32

static int indirectAlias (
    char	*alias,
    ARRAY8Ptr	clientAddress,
    CARD16	connectionType,
    ChooserFunc	function,
    char	*closure,
    int		depth,
    int		broadcast);


static int
scanHostlist (
    HostEntry	*h,
    ARRAY8Ptr	clientAddress,
    CARD16	connectionType,
    ChooserFunc	function,
    char	*closure,
    int		depth,
    int		broadcast)
{
    int	haveLocalhost = 0;

    for (; h; h = h->next)
    {
	switch (h->type) {
	case HOST_ALIAS:
	    if (indirectAlias (h->entry.aliasName, clientAddress,
			       connectionType, function, closure, depth,
			       broadcast))
		haveLocalhost = 1;
	    break;
	case HOST_ADDRESS:
	    if (XdmcpARRAY8Equal (getLocalAddress(), &h->entry.hostAddress))
		haveLocalhost = 1;
	    else if (function)
		(*function) (connectionType, &h->entry.hostAddress, closure);
	    break;
	case HOST_BROADCAST:
	    if (broadcast)
	    {
		ARRAY8	temp;

		if (function)
		{
		    temp.data = (BYTE *) BROADCAST_STRING;
		    temp.length = strlen ((char *)temp.data);
		    (*function) (connectionType, &temp, closure);
		}
	    }
	    break;
	}
    }
    return haveLocalhost;
}

/* Returns non-0 iff string is matched by pattern.  Does case folding.
 */
static int
patternMatch (char *string, char *pattern)
{
    int	    p, s;

    if (!string)
	string = "";

    for (;;)
    {
	s = *string++;
	switch (p = *pattern++) {
	case '*':
	    if (!*pattern)
		return 1;
	    for (string--; *string; string++)
		if (patternMatch (string, pattern))
		    return 1;
	    return 0;
	case '?':
	    if (s == '\0')
		return 0;
	    break;
	case '\0':
	    return s == '\0';
	case '\\':
	    p = *pattern++;
	    /* fall through */
	default:
	    if (isupper(p)) p = tolower(p);
	    if (isupper(s)) s = tolower(s);
	    if (p != s)
		return 0;
	}
    }
}

static int
indirectAlias (
    char	*alias,
    ARRAY8Ptr	clientAddress,
    CARD16	connectionType,
    ChooserFunc	function,
    char	*closure,
    int		depth,
    int		broadcast)
{
    DisplayEntry    *d;
    int		    haveLocalhost = 0;

    if (depth == MAX_DEPTH)
	return 0;
    for (d = database; d; d = d->next)
    {
	if (d->type != DISPLAY_ALIAS || !patternMatch (alias, d->entry.aliasName))
	    continue;
	if (scanHostlist (d->hosts, clientAddress, connectionType,
			  function, closure, depth + 1, broadcast))
	{
	    haveLocalhost = 1;
	}
    }
    return haveLocalhost;
}

int ForEachMatchingIndirectHost (
    ARRAY8Ptr	clientAddress,
    CARD16	connectionType,
    ChooserFunc	function,
    char	*closure)
{
    int		    haveLocalhost = 0;
    DisplayEntry    *d;
    char	    *clientName = NULL;

    for (d = database; d; d = d->next)
    {
    	switch (d->type) {
    	case DISPLAY_ALIAS:
	case DISPLAY_LISTEN:
	    continue;
    	case DISPLAY_PATTERN:
	    if (!clientName)
		clientName = NetworkAddressToHostname (connectionType,
						       clientAddress);
	    if (!patternMatch (clientName, d->entry.displayPattern))
		continue;
	    break;
    	case DISPLAY_ADDRESS:
	    if (d->entry.displayAddress.connectionType != connectionType ||
	    	!XdmcpARRAY8Equal (&d->entry.displayAddress.clientAddress,
				  clientAddress))
	    {
		continue;
	    }
	    break;
    	}
	if (!d->hosts)
	    continue;
	if (d->notAllowed)
	    break;
	if (d->chooser)
	{
	    ARRAY8Ptr	choice;

	    choice = IndirectChoice (clientAddress, connectionType);
	    if (!choice || XdmcpARRAY8Equal (getLocalAddress(), choice))
		haveLocalhost = 1;
	    else
		(*function) (connectionType, choice, closure);
	}
	else if (scanHostlist (d->hosts, clientAddress, connectionType,
			  function, closure, 0, FALSE))
	{
	    haveLocalhost = 1;
	}
	break;
    }
    if (clientName)
	free (clientName);
    return haveLocalhost;
}

int UseChooser (
    ARRAY8Ptr	clientAddress,
    CARD16	connectionType)
{
    DisplayEntry    *d;
    char	    *clientName = NULL;

    for (d = database; d; d = d->next)
    {
    	switch (d->type) {
    	case DISPLAY_ALIAS:
	case DISPLAY_LISTEN:
	    continue;
    	case DISPLAY_PATTERN:
	    if (!clientName)
		clientName = NetworkAddressToHostname (connectionType,
						       clientAddress);
	    if (!patternMatch (clientName, d->entry.displayPattern))
		continue;
	    break;
    	case DISPLAY_ADDRESS:
	    if (d->entry.displayAddress.connectionType != connectionType ||
	    	!XdmcpARRAY8Equal (&d->entry.displayAddress.clientAddress,
				  clientAddress))
	    {
		continue;
	    }
	    break;
    	}
	if (!d->hosts)
	    continue;
	if (d->notAllowed)
	    break;
	if (d->chooser && !IndirectChoice (clientAddress, connectionType)) {
	    if (clientName)
		free (clientName);
	    return 1;
	}
	break;
    }
    if (clientName)
	free (clientName);
    return 0;
}

void ForEachChooserHost (
    ARRAY8Ptr	clientAddress,
    CARD16	connectionType,
    ChooserFunc	function,
    char	*closure)
{
    int		    haveLocalhost = 0;
    DisplayEntry    *d;
    char	    *clientName = NULL;

    for (d = database; d; d = d->next)
    {
    	switch (d->type) {
    	case DISPLAY_ALIAS:
	case DISPLAY_LISTEN:
	    continue;
    	case DISPLAY_PATTERN:
	    if (!clientName)
		clientName = NetworkAddressToHostname (connectionType,
						       clientAddress);
	    if (!patternMatch (clientName, d->entry.displayPattern))
		continue;
	    break;
    	case DISPLAY_ADDRESS:
	    if (d->entry.displayAddress.connectionType != connectionType ||
	    	!XdmcpARRAY8Equal (&d->entry.displayAddress.clientAddress,
				  clientAddress))
	    {
		continue;
	    }
	    break;
    	}
	if (!d->hosts)
	    continue;
	if (d->notAllowed)
	    break;
	if (!d->chooser)
	    break;
	if (scanHostlist (d->hosts, clientAddress, connectionType,
			  function, closure, 0, TRUE))
	{
	    haveLocalhost = 1;
	}
	break;
    }
    if (clientName)
	free (clientName);
    if (haveLocalhost)
	(*function) (connectionType, getLocalAddress(), closure);
}

/*
 * returns TRUE if the given client is acceptable to the local host.  The
 * given display client is acceptable if it occurs without a host list.
 */

int AcceptableDisplayAddress (
    ARRAY8Ptr	clientAddress,
    CARD16	connectionType,
    xdmOpCode	type)
{
    DisplayEntry    *d;
    char	    *clientName = NULL;

    if (!*accessFile)
	return 1;
    if (type == INDIRECT_QUERY)
	return 1;
    for (d = database; d; d = d->next)
    {
	if (d->hosts)
	    continue;
    	switch (d->type) {
    	case DISPLAY_ALIAS:
    	case DISPLAY_LISTEN:
	    continue;
    	case DISPLAY_PATTERN:
	    if (!clientName)
		clientName = NetworkAddressToHostname (connectionType,
						       clientAddress);
	    if (!patternMatch (clientName, d->entry.displayPattern))
		continue;
	    break;
    	case DISPLAY_ADDRESS:
	    if (d->entry.displayAddress.connectionType != connectionType ||
	    	!XdmcpARRAY8Equal (&d->entry.displayAddress.clientAddress,
				  clientAddress))
	    {
		continue;
	    }
	    break;
    	}
	break;
    }
    if (clientName)
	free (clientName);
    return (d != 0) && (d->notAllowed == 0)
	&& (type == BROADCAST_QUERY ? d->notBroadcast == 0 : 1);
}

void ForEachListenAddr (
    ListenFunc	listenfunction,
    ListenFunc	mcastfunction,
    void	**closure)
{
    DisplayEntry    *d;
    HostEntry	    *h;
    int		    listenFound = 0;

    for (d = database; d != NULL ; d = d->next)
    {
    	if (d->type == DISPLAY_LISTEN) {
	    listenFound = 1;
	    h = d->hosts;
	    if (h != NULL) {
		(*listenfunction) (&h->entry.hostAddress, closure);
	    }
	    for (h = h->next; h != NULL; h = h->next) {
		(*mcastfunction) (&h->entry.hostAddress, closure);
	    }
	}
    }
    if (!listenFound) {
	(*listenfunction) (NULL, closure);
#if defined(IPv6) && defined(AF_INET6) && defined(XDM_DEFAULT_MCAST_ADDR6)
	{   /* Join default IPv6 Multicast Group */

	    static ARRAY8	defaultMcastAddress;

	    if (defaultMcastAddress.length == 0) {
		struct in6_addr addr6;
	    
		if (inet_pton(AF_INET6,XDM_DEFAULT_MCAST_ADDR6,&addr6) == 1) {
		    if (!XdmcpAllocARRAY8 (&defaultMcastAddress, 
		      sizeof(struct in6_addr))) {
			LogOutOfMem ("ReadHostEntry\n");
			defaultMcastAddress.length = -1;
		    } else {
			memcpy(defaultMcastAddress.data, &addr6, 
			  sizeof(struct in6_addr));
		    }
		} else {
		    defaultMcastAddress.length = -1;
		}
	    }
	    if ( defaultMcastAddress.length == sizeof(struct in6_addr) ) {
		(*mcastfunction) (&defaultMcastAddress, closure);
	    }
	}
#endif
    }
}


#endif /* XDMCP */
