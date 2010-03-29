/*
Copyright 1987, 1998  The Open Group

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
 * Copyright 1990, 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation 
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices,
 * or Digital not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Network Computing Devices, or Digital
 * make no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES, OR DIGITAL BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/*
 * this is miscellaneous OS specific stuff.
 *
 * Catalogue support, alternate servers, and cloneing
 */

#include "xfs-config.h"

#include <X11/Xtrans/Xtrans.h>
#include "osstruct.h"
#include <stdio.h>
#include <stdlib.h>
#define  XK_LATIN1
#include <X11/keysymdef.h>
#ifdef __UNIXOS2__
#define _NFILE 256
#endif
#include "globals.h"
#include "osdep.h"

Bool        drone_server = FALSE;

static int  num_alts;
static AlternateServerPtr alt_servers = (AlternateServerPtr) 0;

/*
 * XXX
 *
 * Catalogue support is absolutely minimal.  Some guts are here, but
 * we don't actually do anything with them so the only one exported is
 * 'all'.  Be warned that other parts of the server may incorrectly
 * assume the catalogue list is global, and will therefore need fixing.
 *
 */

static char *catalogue_name = "all";

static Bool			/* stolen from R4 Match() */
pattern_match(char *pat, int plen, char *string)
{
    register int i,
                l;
    int         j,
                m,
                res;
    register char cp,
                cs;
    int         head,
                tail;

    head = 0;
    tail = plen;

    res = -1;
    for (i = 0; i < head; i++) {
	cp = pat[i];
	if (cp == XK_question) {
	    if (!string[i])
		return res;
	    res = 0;
	} else if (cp != string[i])
	    return res;
    }
    if (head == plen)
	return (string[head] ? res : 1);
    l = head;
    while (++i < tail) {
	/* we just skipped an asterisk */
	j = i;
	m = l;
	while ((cp = pat[i]) != XK_asterisk) {
	    if (!(cs = string[l]))
		return 0;
	    if ((cp != cs) && (cp != XK_question)) {
		m++;
		cp = pat[j];
		if (cp == XK_asterisk) {
		    if (!string[m])
			return 0;
		} else {
		    while ((cs = string[m]) != cp) {
			if (!cs)
			    return 0;
			m++;
		    }
		}
		l = m;
		i = j;
	    }
	    l++;
	    i++;
	}
    }
    m = strlen(&string[l]);
    j = plen - tail;
    if (m < j)
	return 0;
    l = (l + m) - j;
    while ((cp = pat[i])) {
	if ((cp != string[l]) && (cp != XK_question))
	    return 0;
	l++;
	i++;
    }
    return 1;
}

int
ListCatalogues(char *pattern, int patlen, int maxnames, 
	       char **catalogues, int *len)
{
    int         count = 0;
    char       *catlist = NULL;
    int         size = 0;

    if (maxnames) {
	if (pattern_match(pattern, patlen, catalogue_name)) {
	    size = strlen(catalogue_name);
	    catlist = (char *) fsalloc(size + 1);
	    if (!catlist)
		goto bail;
	    *catlist = size;
	    memmove( &catlist[1], catalogue_name, size);
	    size++;		/* for length */
	    count++;
	}
    }
bail:
    *len = size;
    *catalogues = catlist;
    return count;
}

/*
 * check if catalogue list is valid
 */

int
ValidateCatalogues(int *num, char *cats)
{
    char       *c = cats;
    int         i,
                len;

    for (i = 0; i < *num; i++) {
	len = *c++;
	if (strncmp(c, catalogue_name, len)) {
	    *num = i;		/* return bad entry index */
	    return FSBadName;
	}
	c += len;
    }
    return FSSuccess;
}

int
SetAlternateServers(char *list)
{
    char       *t,
               *st;
    AlternateServerPtr alts,
                a;
    int         num,
                i;

    t = list;
    num = 1;
    while (*t) {
	if (*t == ',')
	    num++;
	t++;
    }

    a = alts = (AlternateServerPtr) fsalloc(sizeof(AlternateServerRec) * num);
    if (!alts)
	return FSBadAlloc;

    st = t = list;
    a->namelen = 0;
    while (*t) {
	if (*t == ',') {
	    a->name = (char *) fsalloc(a->namelen);
	    if (!a->name) {
		/* XXX  -- leak */
		return FSBadAlloc;
	    }
	    memmove( a->name, st, a->namelen);
	    a->subset = FALSE;	/* XXX */
	    a++;
	    t++;
	    st = t;
	    a->namelen = 0;
	} else {
	    a->namelen++;
	    t++;
	}
    }
    a->name = (char *) fsalloc(a->namelen);
    if (!a->name) {
	/* XXX  -- leak */
	return FSBadAlloc;
    }
    memmove( a->name, st, a->namelen);
    a->subset = FALSE;		/* XXX */

    for (i = 0; i < num_alts; i++) {
	fsfree((char *) alt_servers[i].name);
    }
    fsfree((char *) alt_servers);
    num_alts = num;
    alt_servers = alts;
    return FSSuccess;
}

int
ListAlternateServers(AlternateServerPtr *svrs)
{
    *svrs = alt_servers;
    return num_alts;
}

/*
 * here's some fun stuff.  in order to cleanly handle becoming overloaded,
 * this allows us to clone ourselves.  the parent keeps the Listen
 * socket open, and sends it to itself.  the child stops listening,
 * and becomes a drone, hanging out till it loses all its clients.
 */

int
CloneMyself(void)
{
    int         child;
    char        old_listen_arg[256];
    char	*arg_ptr = old_listen_arg;
    int         i, j;
    int         lastfdesc;
    char	portnum[20];

    assert(!drone_server);	/* a drone shouldn't hit this */

    if (!CloneSelf)
	return -1;

#ifdef __UNIXOS2__
    NoticeF("cloning of font server not supported under OS/2!\n");
    return(-1);
#endif

    old_listen_arg[0] = '\0';

    lastfdesc = sysconf(_SC_OPEN_MAX) - 1;
    if ( (lastfdesc < 0) || (lastfdesc > MAXSOCKS)) {
	lastfdesc = MAXSOCKS;
    }

    NoticeF("attempting clone...\n");
    chdir("/");
    child = fork();
    if (child == -1) {
	/* failed to fork */
	ErrorF("clone failed to fork()\n");
	return -1;
    }
    /*
     * Note:  they still share the same process group, and killing the parent
     * will take out all the kids as well.  this is considered a feature (at
     * least until i'm convinced otherwise)
     */
    if (child == 0) {
	StopListening();
	NoticeF("clone: child becoming drone\n");
	drone_server = TRUE;
	return 1;
    } else {			/* parent */
	NoticeF("clone: parent revitalizing as %s\n", progname);
	CloseErrors();
	/* XXX should we close stdio as well? */
	for (i = 3; i < lastfdesc; i++)
	{
	    for (j = 0; j < ListenTransCount; j++)
		if (ListenTransFds[j] == i)
		    break;
	    
	    if (j >= ListenTransCount)
		(void) close(i);
	}

	for (i = 0; i < ListenTransCount; i++)
	{
	    int trans_id, fd;
	    char *port;

	    if (!_FontTransGetReopenInfo (ListenTransConns[i],
		&trans_id, &fd, &port))
		continue;

	    sprintf (arg_ptr, "%d/%d/%s", trans_id, fd, port);
	    arg_ptr += strlen (arg_ptr);
	    free (port);

	    if (i < ListenTransCount - 1)
	    {
		strcat (arg_ptr, ",");
		arg_ptr++;
	    }
	}

	sprintf (portnum, "%d", ListenPort);
	if (*old_listen_arg != '\0')
	    execlp(progname, progname,
		   "-ls", old_listen_arg,
		   "-cf", configfilename,
		   "-port", portnum,
		   (void *)NULL);

	InitErrors();		/* reopen errors, since we don't want to lose
				 * this */
	Error("clone failed");
	FatalError("failed to clone self\n");
    }
    /* NOTREACHED */
    return 0;
}
