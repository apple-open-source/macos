/* $Xorg: authutil.c,v 1.5 2001/02/09 02:03:26 xorgcvs Exp $ */
/******************************************************************************


Copyright 1993, 1998  The Open Group

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

Author: Ralph Mor, X Consortium
******************************************************************************/
/* $XFree86: xc/lib/ICE/authutil.c,v 3.9 2002/05/31 18:45:41 dawes Exp $ */

#include <X11/ICE/ICElib.h>
#include "ICElibint.h"
#include <X11/ICE/ICEutil.h>
#include <X11/Xos.h>
#include <sys/stat.h>
#include <errno.h>

#include <time.h>
#define Time_t time_t
#ifdef __UNIXOS2__
extern char* getenv(const char*);
#define link rename
#endif
#ifndef X_NOT_POSIX
#include <unistd.h>
#else
#ifndef WIN32
extern unsigned	sleep ();
#else
#define link rename
#endif
#endif

static Status read_short ();
static Status read_string ();
static Status read_counted_string ();
static Status write_short ();
static Status write_string ();
static Status write_counted_string ();



/*
 * The following routines are for manipulating the .ICEauthority file
 * These are utility functions - they are not part of the standard
 * ICE library specification.
 */

char *
IceAuthFileName ()

{
    static char slashDotICEauthority[] = "/.ICEauthority";
    char    	*name;
    static char	*buf;
    static int	bsize;
    int	    	size;
#if defined(WIN32) || defined(__UNIXOS2__)
#ifndef PATH_MAX
#define PATH_MAX 512
#endif
    char    	dir[PATH_MAX];
#endif

    if ((name = getenv ("ICEAUTHORITY")))
	return (name);

    name = getenv ("HOME");

    if (!name)
    {
#ifdef WIN32
    register char *ptr1;
    register char *ptr2;
    int len1 = 0, len2 = 0;

    if ((ptr1 = getenv("HOMEDRIVE")) && (ptr2 = getenv("HOMEDIR"))) {
	len1 = strlen (ptr1);
	len2 = strlen (ptr2);
    } else if (ptr2 = getenv("USERNAME")) {
	len1 = strlen (ptr1 = "/users/");
	len2 = strlen (ptr2);
    }
    if ((len1 + len2 + 1) < PATH_MAX) {
	sprintf (dir, "%s%s", ptr1, (ptr2) ? ptr2 : "");
	name = dir;
    }
    if (!name)
#endif
#ifdef __UNIXOS2__
	strcpy (dir,"c:");
	name = dir;
	if (!name)
#endif
	return (NULL);
    }

    size = strlen (name) + strlen (&slashDotICEauthority[1]) + 2;

    if (size > bsize)
    {
	if (buf)
	    free (buf);
	buf = malloc ((unsigned) size);
	if (!buf)
	    return (NULL);
	bsize = size;
    }

    strcpy (buf, name);
    strcat (buf, slashDotICEauthority + (name[1] == '\0' ? 1 : 0));

    return (buf);
}



int
IceLockAuthFile (file_name, retries, timeout, dead)

char	*file_name;
int	retries;
int	timeout;
long	dead;

{
    char	creat_name[1025], link_name[1025];
    struct stat	statb;
    Time_t	now;
    int		creat_fd = -1;

    if ((int) strlen (file_name) > 1022)
	return (IceAuthLockError);

    strcpy (creat_name, file_name);
    strcat (creat_name, "-c");
    strcpy (link_name, file_name);
    strcat (link_name, "-l");

    if (stat (creat_name, &statb) != -1)
    {
	now = time ((Time_t *) 0);

	/*
	 * NFS may cause ctime to be before now, special
	 * case a 0 deadtime to force lock removal
	 */

	if (dead == 0 || now - statb.st_ctime > dead)
	{
	    unlink (creat_name);
	    unlink (link_name);
	}
    }
    
    while (retries > 0)
    {
	if (creat_fd == -1)
	{
	    creat_fd = creat (creat_name, 0666);

	    if (creat_fd == -1)
	    {
		if (errno != EACCES)
		    return (IceAuthLockError);
	    }
	    else
		close (creat_fd);
	}

	if (creat_fd != -1)
	{
	    if (link (creat_name, link_name) != -1)
		return (IceAuthLockSuccess);

	    if (errno == ENOENT)
	    {
		creat_fd = -1;	/* force re-creat next time around */
		continue;
	    }

	    if (errno != EEXIST)
		return (IceAuthLockError);
	}

	sleep ((unsigned) timeout);
	--retries;
    }

    return (IceAuthLockTimeout);
}



void
IceUnlockAuthFile (file_name)

char	*file_name;

{
#ifndef WIN32
    char	creat_name[1025];
#endif
    char	link_name[1025];

    if ((int) strlen (file_name) > 1022)
	return;

#ifndef WIN32
    strcpy (creat_name, file_name);
    strcat (creat_name, "-c");
#endif
    strcpy (link_name, file_name);
    strcat (link_name, "-l");

#ifndef WIN32
    unlink (creat_name);
#endif
    unlink (link_name);
}



IceAuthFileEntry *
IceReadAuthFileEntry (auth_file)

FILE	*auth_file;

{
    IceAuthFileEntry   	local;
    IceAuthFileEntry   	*ret;

    local.protocol_name = NULL;
    local.protocol_data = NULL;
    local.network_id = NULL;
    local.auth_name = NULL;
    local.auth_data = NULL;

    if (!read_string (auth_file, &local.protocol_name))
	return (NULL);

    if (!read_counted_string (auth_file,
	&local.protocol_data_length, &local.protocol_data))
	goto bad;

    if (!read_string (auth_file, &local.network_id))
	goto bad;

    if (!read_string (auth_file, &local.auth_name))
	goto bad;

    if (!read_counted_string (auth_file,
	&local.auth_data_length, &local.auth_data))
	goto bad;

    if (!(ret = (IceAuthFileEntry *) malloc (sizeof (IceAuthFileEntry))))
	goto bad;

    *ret = local;

    return (ret);

 bad:

    if (local.protocol_name) free (local.protocol_name);
    if (local.protocol_data) free (local.protocol_data);
    if (local.network_id) free (local.network_id);
    if (local.auth_name) free (local.auth_name);
    if (local.auth_data) free (local.auth_data);

    return (NULL);
}



void
IceFreeAuthFileEntry (auth)

IceAuthFileEntry	*auth;

{
    if (auth)
    {
	if (auth->protocol_name) free (auth->protocol_name);
	if (auth->protocol_data) free (auth->protocol_data);
	if (auth->network_id) free (auth->network_id);
	if (auth->auth_name) free (auth->auth_name);
	if (auth->auth_data) free (auth->auth_data);
	free ((char *) auth);
    }
}



Status
IceWriteAuthFileEntry (auth_file, auth)

FILE			*auth_file;
IceAuthFileEntry	*auth;

{
    if (!write_string (auth_file, auth->protocol_name))
	return (0);

    if (!write_counted_string (auth_file,
	auth->protocol_data_length, auth->protocol_data))
	return (0);

    if (!write_string (auth_file, auth->network_id))
	return (0);

    if (!write_string (auth_file, auth->auth_name))
	return (0);

    if (!write_counted_string (auth_file,
	auth->auth_data_length, auth->auth_data))
	return (0);

    return (1);
}



IceAuthFileEntry *
IceGetAuthFileEntry (protocol_name, network_id, auth_name)

char	*protocol_name;
char	*network_id;
char	*auth_name;

{
    FILE    		*auth_file;
    char    		*filename;
    IceAuthFileEntry    *entry;

    if (!(filename = IceAuthFileName ()))
	return (NULL);

    if (access (filename, R_OK) != 0)		/* checks REAL id */
	return (NULL);

    if (!(auth_file = fopen (filename, "rb")))
	return (NULL);

    for (;;)
    {
	if (!(entry = IceReadAuthFileEntry (auth_file)))
	    break;

	if (strcmp (protocol_name, entry->protocol_name) == 0 &&
	    strcmp (network_id, entry->network_id) == 0 &&
            strcmp (auth_name, entry->auth_name) == 0)
	{
	    break;
	}

	IceFreeAuthFileEntry (entry);
    }

    fclose (auth_file);

    return (entry);
}



/*
 * local routines
 */

static Status
read_short (file, shortp)

FILE		*file;
unsigned short	*shortp;

{
    unsigned char   file_short[2];

    if (fread ((char *) file_short, (int) sizeof (file_short), 1, file) != 1)
	return (0);

    *shortp = file_short[0] * 256 + file_short[1];
    return (1);
}


static Status
read_string (file, stringp)

FILE	*file;
char	**stringp;

{
    unsigned short  len;
    char	    *data;

    if (!read_short (file, &len))
	return (0);

    if (len == 0)
    {
	data = 0;
    }
    else
    {
    	data = malloc ((unsigned) len + 1);

    	if (!data)
	    return (0);

    	if (fread (data, (int) sizeof (char), (int) len, file) != len)
	{
	    free (data);
	    return (0);
    	}

	data[len] = '\0';
    }

    *stringp = data;

    return (1);
}


static Status
read_counted_string (file, countp, stringp)

FILE	*file;
unsigned short	*countp;
char	**stringp;

{
    unsigned short  len;
    char	    *data;

    if (!read_short (file, &len))
	return (0);

    if (len == 0)
    {
	data = 0;
    }
    else
    {
    	data = malloc ((unsigned) len);

    	if (!data)
	    return (0);

    	if (fread (data, (int) sizeof (char), (int) len, file) != len)
	{
	    free (data);
	    return (0);
    	}
    }

    *stringp = data;
    *countp = len;

    return (1);
}


static Status
write_short (file, s)

FILE		*file;
unsigned short	s;

{
    unsigned char   file_short[2];

    file_short[0] = (s & (unsigned) 0xff00) >> 8;
    file_short[1] = s & 0xff;

    if (fwrite ((char *) file_short, (int) sizeof (file_short), 1, file) != 1)
	return (0);

    return (1);
}


static Status
write_string (file, string)

FILE		*file;
char		*string;

{
    unsigned short count = strlen (string);

    if (!write_short (file, count))
	return (0);

    if (fwrite (string, (int) sizeof (char), (int) count, file) != count)
	return (0);

    return (1);
}


static Status
write_counted_string (file, count, string)

FILE		*file;
unsigned short	count;
char		*string;

{
    if (!write_short (file, count))
	return (0);

    if (fwrite (string, (int) sizeof (char), (int) count, file) != count)
	return (0);

    return (1);
}
