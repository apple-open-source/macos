/* $Xorg: config.c,v 1.4 2001/02/09 02:05:34 xorgcvs Exp $ */

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
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.
*/
/* $XFree86: xc/programs/proxymngr/config.c,v 1.7 2001/12/14 20:01:01 dawes Exp $ */

#include <stdlib.h>

#include "pmint.h"
#include "config.h"

static Bool
getnextline (
char	**pbuf,
int	*plen,
FILE	*f)

{
    int c, i;

    i = 0;
    while(1)
    {
	if (i+2 > *plen)
	{
	    if (*plen)
		*plen *= 2;
	    else
		*plen = BUFSIZ;
	    if (*pbuf)
		*pbuf = (char *) realloc (*pbuf, *plen + 1);
	    else
		*pbuf = (char *) malloc (*plen + 1);
	    if (! *pbuf) {
		fprintf (stderr, "Memory allocation failure reading config file\n");
		return 0;
	    }
	}
	c = getc (f);
	if (c == EOF)
	    break;
	(*pbuf)[i++] = c;
	if (c == '\n') {
	    i--;
	    break;
	}
    }
    (*pbuf)[i] = '\0';
    return i;
}


#ifdef NEED_STRCASECMP
int
ncasecmp (str1, str2, n)
    char *str1, *str2;
    int n;
{
    char buf1[512],buf2[512];
    char c, *s;
    register int i;

    for (i=0, s = buf1; i < n && (c = *str1++); i++) {
	if (isupper(c))
	    c = tolower(c);
	if (i>510)
	    break;
	*s++ = c;
    }
    *s = '\0';
    for (i=0, s = buf2; i < n && (c = *str2++); i++) {
	if (isupper(c))
	    c = tolower(c);
	if (i>510)
	    break;
	*s++ = c;
    }
    *s = '\0';
    return (strncmp(buf1, buf2, n));
}
#endif /* NEED_STRCASECMP */


Status
GetConfig (
    char *configFile,
    char *serviceName,
    Bool *managed,
    char **startCommand,
    char **proxyAddress)

{
    FILE *fp;
    int found = 0;
    char *buf, *p;
    int buflen, n;

    *startCommand = *proxyAddress = NULL;

    fp = fopen (configFile, "r");

    if (!fp)
	return 0;

    buf = NULL;
    buflen = 0;
    n = strlen (serviceName);

    while (!found && getnextline (&buf, &buflen, fp))
    {
	if (buf[0] == '!')
	    continue;

	if (!(ncasecmp (buf, serviceName, n) == 0 && buf[n] == ' '))
	    continue;

	/* found the right config line */
	p = buf + n + 1;
	while (*p == ' ')
	    p++;
	if (ncasecmp (p, "managed", 7) == 0)
	{
	    *managed = 1;
	    p += 7;
	}
	else if (ncasecmp (p, "unmanaged", 9) == 0)
	{
	    *managed = 0;
	    p += 9;
	}
	else
	{
	    fprintf (stderr, "Error in config file at line \"%s\"\n", buf);
	    break;
	}

	while (*p == ' ')
	    p++;

	if (*managed)
	{
	    n = strlen (p);
	    *startCommand = (char *) malloc (n + 2);
	    if (! *startCommand) {
		fprintf (stderr,
			 "Memory allocation failed for service \"%s\"\n",
			 serviceName);
		break;
	    }
	    strcpy (*startCommand, p);
	    (*startCommand)[n] = '&';
	    (*startCommand)[n + 1] = '\0';
	}
	else
	{
	    *proxyAddress = (char *) malloc (strlen (p) + 1);
	    if (! *proxyAddress) {
		fprintf (stderr,
			 "Memory allocation failed for service \"%s\" at %s\n",
			 serviceName, p);
		break;
	    }
	    strcpy (*proxyAddress, p);
	}

	found = 1;
    }

    if (buf)
	free (buf);

    fclose (fp);
    return found;
}




