/******************************************************************************

Copyright 1994, 1998  The Open Group

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

Author:  Ralph Mor, X Consortium
******************************************************************************/

#include "smproxy.h"
#ifdef HAVE_MKSTEMP
#include <unistd.h>
#endif


static ProxyFileEntry *proxyFileHead = NULL;

static int write_byte ( FILE *file, unsigned char b );
static int write_short ( FILE *file, unsigned short s );
static int write_counted_string ( FILE *file, char *string );
static int read_byte ( FILE *file, unsigned char *bp );
static int read_short ( FILE *file, unsigned short *shortp );
static int read_counted_string ( FILE *file, char **stringp );
#ifndef HAVE_MKSTEMP
static char * unique_filename ( char *path, char *prefix );
#else
static char * unique_filename ( char *path, char *prefix, int *pFd );
#endif


static int
write_byte (FILE *file, unsigned char b)
{
    if (fwrite ((char *) &b, 1, 1, file) != 1)
	return 0;
    return 1;
}


static int
write_short (FILE *file, unsigned short s)
{
    unsigned char   file_short[2];

    file_short[0] = (s & (unsigned)0xff00) >> 8;
    file_short[1] = s & 0xff;
    if (fwrite ((char *) file_short, (int) sizeof (file_short), 1, file) != 1)
	return 0;
    return 1;
}


static int
write_counted_string(FILE *file, char *string)
{
    if (string)
    {
	unsigned char count = strlen (string);

	if (write_byte (file, count) == 0)
	    return 0;
	if (fwrite (string, (int) sizeof (char), (int) count, file) != count)
	    return 0;
    }
    else
    {
	if (write_byte (file, 0) == 0)
	    return 0;
    }

    return 1;
}



static int
read_byte(FILE *file, unsigned char *bp)
{
    if (fread ((char *) bp, 1, 1, file) != 1)
	return 0;
    return 1;
}


static int
read_short(FILE *file, unsigned short *shortp)
{
    unsigned char   file_short[2];

    if (fread ((char *) file_short, (int) sizeof (file_short), 1, file) != 1)
	return 0;
    *shortp = file_short[0] * 256 + file_short[1];
    return 1;
}


static int
read_counted_string(FILE *file, char **stringp)
{
    unsigned char  len;
    char	   *data;

    if (read_byte (file, &len) == 0)
	return 0;
    if (len == 0) {
	data = NULL;
    } else {
    	data = (char *) malloc ((unsigned) len + 1);
    	if (!data)
	    return 0;
    	if (fread (data, (int) sizeof (char), (int) len, file) != len) {
	    free (data);
	    return 0;
    	}
	data[len] = '\0';
    }
    *stringp = data;
    return 1;
}



/*
 * An entry in the .smproxy file looks like this:
 *
 * FIELD				BYTES
 * -----                                ----
 * client ID len			1
 * client ID				LIST of bytes
 * WM_CLASS "res name" length		1
 * WM_CLASS "res name"			LIST of bytes
 * WM_CLASS "res class" length          1
 * WM_CLASS "res class"                 LIST of bytes
 * WM_NAME length			1
 * WM_NAME				LIST of bytes
 * WM_COMMAND arg count			1
 * For each arg in WM_COMMAND
 *    arg length			1
 *    arg				LIST of bytes
 */

int
WriteProxyFileEntry(FILE *proxyFile, WinInfo *theWindow)
{
    int i;

    if (!write_counted_string (proxyFile, theWindow->client_id))
	return 0;
    if (!write_counted_string (proxyFile, theWindow->class.res_name))
	return 0;
    if (!write_counted_string (proxyFile, theWindow->class.res_class))
	return 0;
    if (!write_counted_string (proxyFile, theWindow->wm_name))
	return 0;
    
    if (!theWindow->wm_command || theWindow->wm_command_count == 0)
    {
	if (!write_byte (proxyFile, 0))
	    return 0;
    }
    else
    {
	if (!write_byte (proxyFile, (char) theWindow->wm_command_count))
	    return 0;
	for (i = 0; i < theWindow->wm_command_count; i++)
	    if (!write_counted_string (proxyFile, theWindow->wm_command[i]))
		return 0;
    }

    return 1;
}


int
ReadProxyFileEntry(FILE *proxyFile, ProxyFileEntry **pentry)
{
    ProxyFileEntry *entry;
    unsigned char byte;
    int i;

    *pentry = entry = (ProxyFileEntry *) malloc (
	sizeof (ProxyFileEntry));
    if (!*pentry)
	return 0;

    entry->tag = 0;
    entry->client_id = NULL;
    entry->class.res_name = NULL;
    entry->class.res_class = NULL;
    entry->wm_name = NULL;
    entry->wm_command = NULL;
    entry->wm_command_count = 0;

    if (!read_counted_string (proxyFile, &entry->client_id))
	goto give_up;
    if (!read_counted_string (proxyFile, &entry->class.res_name))
	goto give_up;
    if (!read_counted_string (proxyFile, &entry->class.res_class))
	goto give_up;
    if (!read_counted_string (proxyFile, &entry->wm_name))
	goto give_up;
    
    if (!read_byte (proxyFile, &byte))
	goto give_up;
    entry->wm_command_count = byte;

    if (entry->wm_command_count == 0)
	entry->wm_command = NULL;
    else
    {
	entry->wm_command = (char **) malloc (entry->wm_command_count *
	    sizeof (char *));

	if (!entry->wm_command)
	    goto give_up;

	for (i = 0; i < entry->wm_command_count; i++)
	    if (!read_counted_string (proxyFile, &entry->wm_command[i]))
		goto give_up;
    }

    return 1;

give_up:

    if (entry->client_id)
	free (entry->client_id);
    if (entry->class.res_name)
	free (entry->class.res_name);
    if (entry->class.res_class)
	free (entry->class.res_class);
    if (entry->wm_name)
	free (entry->wm_name);
    if (entry->wm_command_count)
    {
	for (i = 0; i < entry->wm_command_count; i++)
	    if (entry->wm_command[i])
		free (entry->wm_command[i]);
    }
    if (entry->wm_command)
	free ((char *) entry->wm_command);
    
    free ((char *) entry);
    *pentry = NULL;

    return 0;
}


void
ReadProxyFile(char *filename)
{
    FILE *proxyFile;
    ProxyFileEntry *entry;
    int done = 0;
    unsigned short version;

    proxyFile = fopen (filename, "rb");
    if (!proxyFile)
	return;

    if (!read_short (proxyFile, &version) ||
	version > SAVEFILE_VERSION)
    {
	done = 1;
    }

    while (!done)
    {
	if (ReadProxyFileEntry (proxyFile, &entry))
	{
	    entry->next = proxyFileHead;
	    proxyFileHead = entry;
	}
	else
	    done = 1;
    }

    fclose (proxyFile);
}



#ifndef HAVE_MKSTEMP
static char *
unique_filename(char *path, char *prefix)
#else
static char *
unique_filename(char *path, char *prefix, int *pFd)
#endif
{
#ifndef HAVE_MKSTEMP
#ifndef X_NOT_POSIX
    return ((char *) tempnam (path, prefix));
#else
    char tempFile[PATH_MAX];
    char *tmp;

    sprintf (tempFile, "%s/%sXXXXXX", path, prefix);
    tmp = (char *) mktemp (tempFile);
    if (tmp)
    {
	char *ptr = (char *) malloc (strlen (tmp) + 1);
	strcpy (ptr, tmp);
	return (ptr);
    }
    else
	return (NULL);
#endif
#else 
    char tempFile[PATH_MAX];
    char *ptr;

    sprintf (tempFile, "%s/%sXXXXXX", path, prefix);
    ptr = (char *)malloc(strlen(tempFile) + 1);
    if (ptr != NULL) 
    {
	strcpy(ptr, tempFile);
	*pFd =  mkstemp(ptr);
    }
    return ptr;
#endif
}



char *
WriteProxyFile(void)
{
    FILE *proxyFile = NULL;
    char *filename = NULL;
#ifdef HAVE_MKSTEMP
    int fd;
#endif
    char *path;
    WinInfo *winptr;
    Bool success = False;

    path = getenv ("SM_SAVE_DIR");
    if (!path)
    {
	path = getenv ("HOME");
	if (!path)
	    path = ".";
    }

#ifndef HAVE_MKSTEMP
    if ((filename = unique_filename (path, ".prx")) == NULL)
	goto bad;

    if (!(proxyFile = fopen (filename, "wb")))
	goto bad;
#else
    if ((filename = unique_filename (path, ".prx", &fd)) == NULL)
	goto bad;

    if (!(proxyFile = fdopen(fd, "wb"))) 
	goto bad;
#endif
    if (!write_short (proxyFile, SAVEFILE_VERSION))
	goto bad;

    success = True;
    winptr = win_head;

    while (winptr && success)
    {
	if (winptr->client_id)
	    if (!WriteProxyFileEntry (proxyFile, winptr))
	    {
		success = False;
		break;
	    }

	winptr = winptr->next;
    }

 bad:

    if (proxyFile)
	fclose (proxyFile);

    if (success)
	return (filename);
    else
    {
	if (filename)
	    free (filename);
	return (NULL);
    }
}



char *
LookupClientID(WinInfo *theWindow)
{
    ProxyFileEntry *ptr;
    int found = 0;

    ptr = proxyFileHead;
    while (ptr && !found)
    {
	if (!ptr->tag &&
            strcmp (theWindow->class.res_name, ptr->class.res_name) == 0 &&
	    strcmp (theWindow->class.res_class, ptr->class.res_class) == 0 &&
	    strcmp (theWindow->wm_name, ptr->wm_name) == 0)
	{
	    int i;

	    if (theWindow->wm_command_count == ptr->wm_command_count)
	    {
		for (i = 0; i < theWindow->wm_command_count; i++)
		    if (strcmp (theWindow->wm_command[i],
			ptr->wm_command[i]) != 0)
			break;

		if (i == theWindow->wm_command_count)
		    found = 1;
	    }
	}

	if (!found)
	    ptr = ptr->next;
    }

    if (found)
    {
	ptr->tag = 1;
	return (ptr->client_id);
    }
    else
	return NULL;
}
