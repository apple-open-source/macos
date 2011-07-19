/*
 *
Copyright 1990, 1998  The Open Group

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

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 *
 * Author:  Keith Packard, MIT X Consortium
 */


/*
 * choose.c
 *
 * xdm interface to chooser program
 */

#include "dm.h"
#include "dm_error.h"

#ifdef XDMCP

# include <X11/X.h>
# include <sys/types.h>

# include "dm_socket.h"
# include <arpa/inet.h>

# ifndef X_NO_SYS_UN
#  include <sys/un.h>
# endif

# include <ctype.h>
# include <errno.h>

# if defined(STREAMSCONN)
#  include       <tiuser.h>
# endif

# include <time.h>
# define Time_t time_t

static int
FormatBytes (
    unsigned char *data,
    int	    length,
    char    *buf,
    int	    buflen)
{
    int	    i;
    static char	    HexChars[] = "0123456789abcdef";

    if (buflen < length * 2 + 1)
	return 0;
    for (i = 0; i < length; i++)
    {
	*buf++ = HexChars[(data[i] >> 4) & 0xf];
	*buf++ = HexChars[(data[i]) & 0xf];
    }
    *buf++ = '\0';
    return 1;
}

static int
FormatARRAY8 (
    ARRAY8Ptr	a,
    char	*buf,
    int		buflen)
{
    return FormatBytes (a->data, a->length, buf, buflen);
}

/* Converts an Internet address in ARRAY8 format to a string in
   familiar dotted address notation, e.g., "18.24.0.11"
   Returns 1 if successful, 0 if not.
   */
static int
ARRAY8ToDottedDecimal (
    ARRAY8Ptr	a,
    char	*buf,
    int		buflen)
{
    int outlen;
    if (a->length != 4)
	return 0;
    outlen = snprintf(buf, buflen, "%d.%d.%d.%d",
		      a->data[0], a->data[1], a->data[2], a->data[3]);
    if (outlen >= buflen) {
	return 0;
    }
    return 1;
}

typedef struct _IndirectUsers {
    struct _IndirectUsers   *next;
    ARRAY8	client;
    CARD16	connectionType;
} IndirectUsersRec, *IndirectUsersPtr;

static IndirectUsersPtr	indirectUsers;

int
RememberIndirectClient (
    ARRAY8Ptr	clientAddress,
    CARD16	connectionType)
{
    IndirectUsersPtr	i;

    for (i = indirectUsers; i; i = i->next)
	if (XdmcpARRAY8Equal (clientAddress, &i->client) &&
	    connectionType == i->connectionType)
	    return 1;
    i = (IndirectUsersPtr) malloc (sizeof (IndirectUsersRec));
    if (!XdmcpCopyARRAY8 (clientAddress, &i->client))
    {
	free ((char *) i);
	return 0;
    }
    i->connectionType = connectionType;
    i->next = indirectUsers;
    indirectUsers = i;
    return 1;
}

void
ForgetIndirectClient (
    ARRAY8Ptr	clientAddress,
    CARD16	connectionType)
{
    IndirectUsersPtr	i, prev;

    prev = NULL;
    for (i = indirectUsers; i; i = i->next)
    {
	if (XdmcpARRAY8Equal (clientAddress, &i->client) &&
	    connectionType == i->connectionType)
	{
	    if (prev)
		prev->next = i->next;
	    else
		indirectUsers = i->next;
	    XdmcpDisposeARRAY8 (&i->client);
	    free ((char *) i);
	    break;
	}
	prev = i;
    }
}

int
IsIndirectClient (
    ARRAY8Ptr	clientAddress,
    CARD16	connectionType)
{
    IndirectUsersPtr	i;

    for (i = indirectUsers; i; i = i->next)
	if (XdmcpARRAY8Equal (clientAddress, &i->client) &&
	    connectionType == i->connectionType)
	    return 1;
    return 0;
}

static int
FormatChooserArgument (char *buf, int len)
{
    unsigned char   addr_buf[1024];
    int		    addr_len = sizeof (addr_buf);
    unsigned char   result_buf[1024];
    int		    result_len = 0;
    int		    netfamily;

    if (GetChooserAddr ((char *)addr_buf, &addr_len) == -1)
    {
	LogError ("Cannot get return address for chooser socket\n");
	Debug ("Cannot get chooser socket address\n");
	return 0;
    }
    netfamily = NetaddrFamily((XdmcpNetaddr)addr_buf);
    switch (netfamily) {
    case AF_INET:
# if defined(IPv6) && defined(AF_INET6)
    case AF_INET6:
# endif
	{
	    char *port;
	    int portlen;
	    ARRAY8Ptr localAddress = getLocalAddress ();

# if defined(IPv6) && defined(AF_INET6)
	    if (localAddress->length == 16)
		netfamily = AF_INET6;
	    else
		netfamily = AF_INET;
# endif

	    port = NetaddrPort((XdmcpNetaddr)addr_buf, &portlen);
	    result_buf[0] = netfamily >> 8;
	    result_buf[1] = netfamily & 0xFF;
	    result_buf[2] = port[0];
	    result_buf[3] = port[1];
	    memmove( (char *)result_buf+4, (char *)localAddress->data,
	      localAddress->length);
	    result_len = 4 + localAddress->length;
	}
	break;
# ifdef AF_DECnet
    case AF_DECnet:
	break;
# endif
    default:
	Debug ("Chooser family %d isn't known\n", netfamily);
	return 0;
    }

    return FormatBytes (result_buf, result_len, buf, len);
}

typedef struct _Choices {
    struct _Choices *next;
    ARRAY8	    client;
    CARD16	    connectionType;
    ARRAY8	    choice;
    Time_t	    time;
} ChoiceRec, *ChoicePtr;

static ChoicePtr   choices;

ARRAY8Ptr
IndirectChoice (
    ARRAY8Ptr	clientAddress,
    CARD16	connectionType)
{
    ChoicePtr	c, next, prev;
    Time_t	now;

    now = time ((Time_t*)0);
    prev = NULL;
    for (c = choices; c; c = next)
    {
	next = c->next;
	Debug ("Choice checking timeout: %ld >? %d\n",
	    (long)(now - c->time), choiceTimeout);
	if (now - c->time > (Time_t)choiceTimeout)
	{
	    Debug ("Timeout choice %ld > %d\n",
		(long)(now - c->time), choiceTimeout);
	    if (prev)
		prev->next = next;
	    else
		choices = next;
	    XdmcpDisposeARRAY8 (&c->client);
	    XdmcpDisposeARRAY8 (&c->choice);
	    free ((char *) c);
	}
	else
	{
	    if (XdmcpARRAY8Equal (clientAddress, &c->client) &&
	    	connectionType == c->connectionType)
	    	return &c->choice;
	    prev = c;
	}
    }
    return NULL;
}

static int
RegisterIndirectChoice (
    ARRAY8Ptr	clientAddress,
    CARD16      connectionType,
    ARRAY8Ptr	choice)
{
    ChoicePtr	c;
    int		insert;
# if 0
    int		found = 0;
# endif

    Debug ("Got indirect choice back\n");
    for (c = choices; c; c = c->next) {
	if (XdmcpARRAY8Equal (clientAddress, &c->client) &&
	    connectionType == c->connectionType) {
# if 0
	    found = 1;
# endif
	    break;
	}
    }
# if 0
    if (!found)
	return 0;
# endif

    insert = 0;
    if (!c)
    {
	c = (ChoicePtr) malloc (sizeof (ChoiceRec));
	insert = 1;
	if (!c)
	    return 0;
	c->connectionType = connectionType;
    	if (!XdmcpCopyARRAY8 (clientAddress, &c->client))
    	{
	    free ((char *) c);
	    return 0;
    	}
    }
    else
    {
	XdmcpDisposeARRAY8 (&c->choice);
    }
    if (!XdmcpCopyARRAY8 (choice, &c->choice))
    {
	XdmcpDisposeARRAY8 (&c->client);
	free ((char *) c);
	return 0;
    }
    if (insert)
    {
	c->next = choices;
	choices = c;
    }
    c->time = time ((Time_t *) 0);
    return 1;
}

# ifdef notdef
static
RemoveIndirectChoice (clientAddress, connectionType)
    ARRAY8Ptr	clientAddress;
    CARD16	connectionType;
{
    ChoicePtr	c, prev;

    prev = 0;
    for (c = choices; c; c = c->next)
    {
	if (XdmcpARRAY8Equal (clientAddress, &c->client) &&
	    connectionType == c->connectionType)
	{
	    if (prev)
		prev->next = c->next;
	    else
		choices = c->next;
	    XdmcpDisposeARRAY8 (&c->client);
	    XdmcpDisposeARRAY8 (&c->choice);
	    free ((char *) c);
	    return;
	}
	prev = c;
    }
}
# endif

/*ARGSUSED*/
static void
AddChooserHost (
    CARD16	connectionType,
    ARRAY8Ptr	addr,
    char	*closure)
{
    char	***argp;
    char	hostbuf[1024];

    argp = (char ***) closure;
    if (addr->length == strlen ("BROADCAST") &&
	!strncmp ((char *)addr->data, "BROADCAST", addr->length))
    {
	*argp = parseArgs (*argp, "BROADCAST");
    }
# if defined(IPv6) && defined(AF_INET6)
    else if ( (addr->length == 16) &&
      (inet_ntop(AF_INET6, addr->data, hostbuf, sizeof(hostbuf))))
    {
	*argp = parseArgs (*argp, hostbuf);
    }
# endif
    else if (ARRAY8ToDottedDecimal (addr, hostbuf, sizeof (hostbuf)))
    {
	*argp = parseArgs (*argp, hostbuf);
    }
}

void
ProcessChooserSocket (int fd)
{
    int client_fd;
    char	buf[1024];
    int		len;
    XdmcpBuffer	buffer;
    ARRAY8	clientAddress = {0, NULL};
    CARD16	connectionType;
    ARRAY8	choice = {0, NULL};
# if defined(STREAMSCONN)
    struct t_call *call;
    int flags=0;
# endif

    Debug ("Process chooser socket\n");
    len = sizeof (buf);
# if defined(STREAMSCONN)
    call = (struct t_call *)t_alloc( fd, T_CALL, T_ALL );
    if( call == NULL )
    {
	t_error( "ProcessChooserSocket: t_alloc failed" );
	LogError ("Cannot setup to listen on chooser connection\n");
	return;
    }
    if( t_listen( fd, call ) < 0 )
    {
	t_error( "ProcessChooserSocket: t_listen failed" );
	t_free( (char *)call, T_CALL );
	LogError ("Cannot listen on chooser connection\n");
	return;
    }
    client_fd = t_open ("/dev/tcp", O_RDWR, NULL);
    if (client_fd == -1)
    {
	t_error( "ProcessChooserSocket: t_open failed" );
	t_free( (char *)call, T_CALL );
	LogError ("Cannot open new chooser connection\n");
	return;
    }
    if( t_bind( client_fd, NULL, NULL ) < 0 )
    {
	t_error( "ProcessChooserSocket: t_bind failed" );
	t_free( (char *)call, T_CALL );
	LogError ("Cannot bind new chooser connection\n");
        t_close (client_fd);
	return;
    }
    if( t_accept (fd, client_fd, call) < 0 )
    {
	t_error( "ProcessChooserSocket: t_accept failed" );
	LogError ("Cannot accept chooser connection\n");
	t_free( (char *)call, T_CALL );
        t_unbind (client_fd);
        t_close (client_fd);
	return;
    }
# else
    client_fd = accept (fd, (struct sockaddr *)buf, (void *)&len);
    if (client_fd == -1)
    {
	LogError ("Cannot accept chooser connection\n");
	return;
    }
# endif
    Debug ("Accepted %d\n", client_fd);

# if defined(STREAMSCONN)
    len = t_rcv (client_fd, buf, sizeof (buf),&flags);
# else
    len = read (client_fd, buf, sizeof (buf));
# endif
    Debug ("Read returns %d\n", len);
    if (len > 0)
    {
    	buffer.data = (BYTE *) buf;
    	buffer.size = sizeof (buf);
    	buffer.count = len;
    	buffer.pointer = 0;
	if (XdmcpReadARRAY8 (&buffer, &clientAddress)) {
	    if (XdmcpReadCARD16 (&buffer, &connectionType)) {
		if (XdmcpReadARRAY8 (&buffer, &choice)) {
		    Debug ("Read from chooser succesfully\n");
		    RegisterIndirectChoice (&clientAddress, connectionType, &choice);
		    XdmcpDisposeARRAY8 (&choice);
		} else {
		    LogError ("Invalid choice response length %d\n", len);
		}
	    } else {
		LogError ("Invalid choice response length %d\n", len);
	    }
	    XdmcpDisposeARRAY8 (&clientAddress);
	} else {
	    LogError ("Invalid choice response length %d\n", len);
	}
    }
    else
    {
	LogError ("Choice response read error: %s\n", _SysErrorMsg(errno));
    }

# if defined(STREAMSCONN)
    t_unbind (client_fd);
    t_free( (char *)call, T_CALL );
    t_close (client_fd);
# else
    close (client_fd);
# endif
}

void
RunChooser (struct display *d)
{
    char    **args;
    char    buf[1024];
    char    **env;

    Debug ("RunChooser %s\n", d->name);
# ifndef HAS_SETPROCTITLE
    SetTitle (d->name, "chooser", (char *) 0);
# else
    setproctitle("chooser %s", d->name);
# endif
    LoadXloginResources (d);
    args = parseArgs ((char **) 0, d->chooser);
    strcpy (buf, "-xdmaddress ");
    if (FormatChooserArgument (buf + strlen (buf), sizeof (buf) - strlen (buf)))
	args = parseArgs (args, buf);
    strcpy (buf, "-clientaddress ");
    if (FormatARRAY8 (&d->clientAddr, buf + strlen (buf), sizeof (buf) - strlen (buf)))
	args = parseArgs (args, buf);
    snprintf (buf, sizeof(buf), "-connectionType %d", d->connectionType);
    args = parseArgs (args, buf);
    ForEachChooserHost (&d->clientAddr, d->connectionType, AddChooserHost,
			(char *) &args);
    env = systemEnv (d, (char *) 0, (char *) 0);
    Debug ("Running %s\n", args[0]);
    execute (args, env);
    Debug ("Couldn't run %s\n", args[0]);
    LogError ("Cannot execute %s\n", args[0]);
    exit (REMANAGE_DISPLAY);
}

#endif /* XDMCP */

