/*
 * misc os utilities
 */
/*
 
Copyright 1990, 1991, 1998  The Open Group

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
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices, or Digital
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES,
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

#include	"xfs-config.h"

#include	<stdio.h>
#include	<X11/Xos.h>
#include	<stdlib.h>
#include	"misc.h"
#include	"globals.h"
#include	<signal.h>
#include	<sys/wait.h>
#include	<unistd.h>
#include	<pwd.h>
#include	<grp.h>
#include	<errno.h>
#include	<sys/types.h>
#include	<errno.h>
#include	<string.h>
#include	"osdep.h"

#include <stdlib.h>

static Bool dropPriv = FALSE; /* whether or not to drop root privileges */
#ifdef DEFAULT_DAEMON
static Bool becomeDaemon = TRUE; /* whether to become a daemon or not */
#else
static Bool becomeDaemon = FALSE; /* whether to become a daemon or not */
#endif
#ifdef XFS_INETD
static Bool runFromInetd = FALSE; /* whether we were run from inetd or not */
#endif
static const char *userId = NULL;
char       *progname;
Bool        CloneSelf;
Bool        portFromCmdline = FALSE;

OldListenRec *OldListen = NULL;
int 	     OldListenCount = 0;

#ifndef STDERR_FILENO
# define STDERR_FILENO fileno(stderr)
#endif
#define WRITES(s) write(STDERR_FILENO, s, strlen(s))

static char *pidFile = XFSPIDDIR "/xfs.pid";
static int  pidFd;
static FILE *pidFilePtr;
static int  StorePid (void);

/* ARGSUSED */
void
AutoResetServer(int n)
{
    int olderrno = errno;

#ifdef DEBUG
    WRITES("got a reset signal\n");
#endif

    dispatchException |= DE_RESET;
    isItTimeToYield = TRUE;

    errno = olderrno;
}

/* ARGSUSED */
void
GiveUp(int n)
{
    int olderrno = errno;
#ifdef DEBUG
    WRITES("got a TERM signal\n");
#endif

    dispatchException |= DE_TERMINATE;
    isItTimeToYield = TRUE;
    errno = olderrno;
}

/* ARGSUSED */
void
ServerReconfig(int n)
{
    int olderrno = errno;

#ifdef DEBUG
    WRITES("got a re-config signal\n");
#endif

    dispatchException |= DE_RECONFIG;
    isItTimeToYield = TRUE;

    errno = olderrno;
}

/* ARGSUSED */
void
ServerCacheFlush(int n)
{
    int olderrno = errno;

#ifdef DEBUG
    WRITES("got a flush signal\n");
#endif

    dispatchException |= DE_FLUSH;
    isItTimeToYield = TRUE;

    errno = olderrno;
}

/* ARGSUSED */
void
CleanupChild(int n)
{
    int olderrno = errno;
    pid_t child;

#ifdef DEBUG
    WRITES("got a child signal\n");
#endif

    while ( (child = waitpid((pid_t)-1, NULL, WNOHANG)) > 0 ) {
#ifdef DEBUG
	char msgbuf[64];

	snprintf(msgbuf, sizeof(msgbuf), " child %d exited\n", child);
	WRITES(msgbuf);
#endif
    }

    errno = olderrno;
}

long
GetTimeInMillis(void)
{
    struct timeval tp;

    X_GETTIMEOFDAY(&tp);
    return ((tp.tv_sec * 1000) + (tp.tv_usec / 1000));
}

static void
usage(void)
{
    fprintf(stderr, "usage: %s [-config config_file] [-port tcp_port] [-droppriv] [-daemon] [-nodaemon] [-user user_name] [-ls listen_socket]\n",
	    progname);
    exit(1);
}

void
OsInitAllocator (void)
{
    return;
}


/*
 * The '-ls' option is used for cloning the font server.
 *
 * We expect a single string of the form...
 *
 *     transport_id/fd/portnum[,transport_id/fd/portnum]...
 *
 * [] denotes optional and ... denotes repitition.
 *
 * The string must be _exactly_ in the above format. 
 */

void
ProcessLSoption (char *str)
{
    char *ptr = str;
    char *slash;
    char number[20];
    int count = 0;
    int len, i;

    while (*ptr != '\0')
    {
	if (*ptr == ',')
	    count++;
	ptr++;
    }
  
    OldListenCount = count + 1;
    OldListen = (OldListenRec *) malloc (
	OldListenCount * sizeof (OldListenRec));
    if (OldListen == NULL) {
	fprintf(stderr, "ProcessLSoption: malloc error\n");
	exit(1);
    }
    ptr = str;

    for (i = 0; i < OldListenCount; i++)
    {
	slash = (char *) strchr (ptr, '/');
	if (slash == NULL) {
	    usage();
	}
	len = slash - ptr;
	strncpy (number, ptr, len);
	number[len] = '\0';
	OldListen[i].trans_id = atoi (number);

	ptr = slash + 1;

	slash = (char *) strchr (ptr, '/');
	if (slash == NULL) {
	    usage();
	}
	len = slash - ptr;
	strncpy (number, ptr, len);
	number[len] = '\0';
	OldListen[i].fd = atoi (number);

	ptr = slash + 1;

	if (i == OldListenCount - 1)
	    OldListen[i].portnum = atoi (ptr);
	else
	{
	    char *comma = (char *) strchr (ptr, ',');
	    if (comma == NULL) {
		usage();
	    }
	    len = comma - ptr;
	    strncpy (number, ptr, len);
	    number[len] = '\0';
	    OldListen[i].portnum = atoi (number);

	    ptr = comma + 1;
	}
    }
}



/* ARGSUSED */
void
ProcessCmdLine(int argc, char **argv)
{
    int         i;

    progname = argv[0];
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-port")) {
	    if (argv[i + 1]) {
		ListenPort = atoi(argv[++i]);
		portFromCmdline = TRUE;
	    } else
		usage();
	} else if (!strcmp(argv[i], "-ls")) {
	    if (argv[i + 1])
		ProcessLSoption (argv[++i]);
	    else
		usage();
	} else if (!strcmp(argv[i], "-droppriv")) {
	        dropPriv = TRUE;
	} else if (!strcmp(argv[i], "-daemon")) {
	        becomeDaemon = TRUE;
	} else if (!strcmp(argv[i], "-nodaemon")) {
	        becomeDaemon = FALSE;
	} else if (!strcmp(argv[i], "-inetd")) {
#ifdef XFS_INETD
		runFromInetd = TRUE;
#else
		FatalError("-inetd specified, but xfs was not built"
			   " with inetd support\n");
#endif
	} else if (!strcmp(argv[i], "-user")) {
	    if (argv[i + 1])
		userId = argv[++i];
	    else
		usage();
	} else if (!strcmp(argv[i], "-cf") || !strcmp(argv[i], "-config")) {
	    if (argv[i + 1])
		configfilename = argv[++i];
	    else
		usage();
	}
	else
	    usage();
    }
}


#ifndef SPECIAL_MALLOC

unsigned long	Must_have_memory;


/* FSalloc -- FS's internal memory allocator.  Why does it return unsigned
 * int * instead of the more common char *?  Well, if you read K&R you'll
 * see they say that alloc must return a pointer "suitable for conversion"
 * to whatever type you really want.  In a full-blown generic allocator
 * there's no way to solve the alignment problems without potentially
 * wasting lots of space.  But we have a more limited problem. We know
 * we're only ever returning pointers to structures which will have to
 * be long word aligned.  So we are making a stronger guarantee.  It might
 * have made sense to make FSalloc return char * to conform with people's
 * expectations of malloc, but this makes lint happier.
 */

pointer
FSalloc (unsigned long amount)
{
    register pointer  ptr;
	
    if ((long)amount < 0)
	return 0;
    if (amount == 0)
	amount++;
    /* aligned extra on long word boundary */
    amount = (amount + 3) & ~3;
    if ((ptr = (pointer)malloc(amount)) != 0)
	return ptr;
    if (Must_have_memory)
	FatalError("out of memory\n");
    return 0;
}

/*****************
 * FScalloc
 *****************/

pointer
FScalloc (unsigned long amount)
{
    pointer ret;

    ret = FSalloc (amount);
    if (ret)
	bzero ((char *) ret, (int) amount);
    return ret;
}

/*****************
 * FSrealloc
 *****************/

pointer
FSrealloc (pointer ptr, unsigned long amount)
{
    if ((long)amount <= 0)
    {
	if (ptr && !amount)
	    free(ptr);
	return 0;
    }
    amount = (amount + 3) & ~3;
    if (ptr)
        ptr = (pointer)realloc((char *)ptr, amount);
    else
	ptr = (pointer)malloc(amount);
    if (ptr)
        return ptr;
    if (Must_have_memory)
	FatalError("out of memory\n");
    return 0;
}
                    
/*****************
 *  FSfree
 *    calls free 
 *****************/    

void
FSfree(pointer ptr)
{
    if (ptr)
	free((char *)ptr); 
}

#endif /* SPECIAL_MALLOC */


void
SetUserId(void)
{
    /* become xfs user (or other specified on command line) if possible */
    if ((geteuid() == 0) && (dropPriv || userId)) {
	const char *user;
	struct passwd *pwent;

	if (!userId)
	    user = "xfs";
	else
	    user = userId;
	pwent = getpwnam(user);
	if (pwent) {
	    if (setgid(pwent->pw_gid)) {
		FatalError("fatal: couldn't set groupid to xfs user's group\n");
	    }
#ifndef QNX4
#ifndef __CYGWIN__
	    if (setgroups(0, NULL)) {
		FatalError("fatal: couldn't drop supplementary groups\n");
	    }
#endif
	    if (initgroups(user, pwent->pw_gid)) {
		FatalError("fatal: couldn't init supplementary groups\n");
	    }
#endif /* QNX4 */
	    if (setuid(pwent->pw_uid)) {
		FatalError("fatal: couldn't set userid to %s user\n", user);
	    }
	}
    } else if (dropPriv || userId) {
	FatalError("fatal: -droppriv or -user flag specified, but xfs not run as root\n");
    }
}


void
SetDaemonState(void)
{
    int	    oldpid;

#ifdef XFS_INETD
    if (runFromInetd) {
	int inetdListener;

	/* fd's 0, 1, & 2 are the initial listen socket provided by inetd,
	 * so dup it and then clear them so stdin/out/err aren't in use.
	 */
	inetdListener = dup(0);
	if (inetdListener == -1) {
	    FatalError("failed to dup inetd socket: %s\n",
		       strerror(errno));
	}
	DetachStdio();

	/* Setup & pass the inetd socket back through the connection setup
	 * code the same way as a cloned listening port
	 */
	OldListenCount = 1;
	OldListen = _FontTransGetInetdListenInfo (inetdListener);
	if (OldListen == NULL) {
	    FatalError("failed to initialize OldListen to inetd socket: %s\n",
		       strerror(errno));
	}
	ListenPort = OldListen[0].portnum;
	NoticeF("accepting listener from inetd on fd %d, port %d\n",
		inetdListener, ListenPort);
	return;
    }
#endif /* XFS_INETD */

    if (becomeDaemon) {
	BecomeDaemon();
	if ((oldpid = StorePid ())) {
	    if (oldpid == -1)
		ErrorF ("error opening process-id file %s\n", pidFile);
	    else
		ErrorF ("process-id file %s indicates another xfs is "
			  "running (pid %d); exiting\n", pidFile, oldpid);
	    exit(1);
	}
    }
}


static int
StorePid (void)
{
    int		oldpid;

    if (pidFile[0] != '\0') {
	pidFd = open (pidFile, O_RDWR);
	if (pidFd == -1 && errno == ENOENT)
	    pidFd = open (pidFile, O_RDWR|O_CREAT, 0666);
	if (pidFd == -1 || !(pidFilePtr = fdopen (pidFd, "r+")))
	{
	    ErrorF ("cannot open process-id file %s: %s\n", pidFile,
		    strerror (errno));
	    return -1;
	}
	if (fscanf (pidFilePtr, "%d\n", &oldpid) != 1)
	    oldpid = -1;
	if (fseek (pidFilePtr, 0L, SEEK_SET) == -1)
	{
	    ErrorF ("cannot seek process-id file %s: %s\n", pidFile,
		     strerror (errno));
	    return -1;
	}
	if (fprintf (pidFilePtr, "%11ld\n", (long) getpid ()) != 12)
	{
	    ErrorF ("cannot write to process-id file %s: %s\n", pidFile,
		    strerror (errno));
	    return -1;
	}
	(void) fflush (pidFilePtr);
	(void) fclose (pidFilePtr);
    }
    return 0;
}
