/* $Xorg: utils.c,v 1.5 2001/02/09 02:05:32 xorgcvs Exp $ */
/***********************************************************

Copyright 1987, 1996, 1998  The Open Group

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


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* $XFree86: xc/programs/lbxproxy/di/utils.c,v 1.15 2002/12/09 04:10:58 tsi Exp $ */

#include "lbx.h"
#include <stdio.h>
#include <stdlib.h>		/* getenv(), {m,re}alloc() */
#ifdef X_POSIX_C_SOURCE
#define _POSIX_C_SOURCE X_POSIX_C_SOURCE
#include <signal.h>
#undef _POSIX_C_SOURCE
#else
#if defined(X_NOT_POSIX) || defined(_POSIX_SOURCE)
#include <signal.h>
#else
#define _POSIX_SOURCE
#include <signal.h>
#undef _POSIX_SOURCE
#endif
#endif
#if !defined(SYSV) && !defined(Lynx) && !defined(QNX4)
#include <sys/resource.h>
#endif

#include <stdarg.h>

static void VErrorF(const char*, va_list);

#ifdef SIGNALRETURNSINT
#define SIGVAL int
#else
#define SIGVAL void
#endif

#include "util.h"
#include "wire.h"
#include "atomcache.h"
#include "proxyopts.h"

#include <stdlib.h>

/*
 * External declarations not in header files
 */
extern char *display_name;
extern char *display;
extern Bool PartialNetwork;
extern int lbxDebug;

extern char protocolMode;
extern Bool reconnectAfterCloseServer;
extern Bool resetAfterLastClient;
extern Bool terminateAfterLastClient;
extern int  lbxTagCacheSize;
extern Bool lbxUseLbx;
extern Bool lbxUseTags;
extern Bool lbxDoSquishing;
extern Bool lbxCompressImages;
extern Bool lbxDoAtomShortCircuiting;
extern Bool lbxDoLbxGfx;
extern Bool compStats;

/*
 * Static vars
 */
static Bool CoreDump;
static Bool Must_have_memory = FALSE;
static char *dev_tty_from_init = NULL;	/* since we need to parse it anyway */

/*
 * Debug stuff
 */
#ifdef DEBUG
#ifndef SPECIAL_MALLOC
#define MEMBUG
#endif
#endif

#ifdef MEMBUG
#define MEM_FAIL_SCALE 100000
long Memory_fail = 0;
#endif

/*
 * Global vars that may get set when the command line opts are parsed.
 */
#ifdef RGB_DB
char *rgbPath = RGB_DB;
#else
char *rgbPath;
#endif

Bool lbxZeroPad = TRUE; /* zero out pad bytes in X requests */

char *atomsFile = DEF_ATOMS_FILE;

Bool lbxWinAttr = TRUE;	/* group GetWindowAttributes/GetGeometry into 1 trip */

Bool lbxDoCmapGrabbing = TRUE; /* do colormap grabbing? */

int lbxMaxMotionEvents = NUM_MOTION_EVENTS;	/* max # motion events */

int min_keep_prop_size = DEF_KEEP_PROP_SIZE;

/* 
 * zlevel = 1..9, 9 == max compression. 6 == good tradeoff between 
 * compression and speed. Try gzipping a large file at the default
 * level (which is 6) and at max compression (9) and notice the
 * difference in time it takes to compress the file and the difference 
 * in file size. level 9 compression takes ~50 more (time, cpu) but 
 * only yields a very small improvement in compression.
 */
int zlevel = 6;		


/*
 * The functions
 */
OsSigHandlerPtr
OsSignal(sig, handler)
    int sig;
    OsSigHandlerPtr handler;
{
#ifdef X_NOT_POSIX
    return signal(sig, handler);
#else
    struct sigaction act, oact;

    sigemptyset(&act.sa_mask);
    if (handler != SIG_IGN)
	sigaddset(&act.sa_mask, sig);
    act.sa_flags = 0;
    act.sa_handler = handler;
    sigaction(sig, &act, &oact);
    return oact.sa_handler;
#endif
}

/* Force connections to close on SIGHUP from init */

/* ARGSUSED */
SIGVAL
AutoResetServer (sig)
    int	sig;
{
    dispatchException |= DE_RESET;
    isItTimeToYield = TRUE;
#ifdef GPROF
    chdir ("/tmp");
    _exit (0);
#endif
#ifdef SYSV
    signal (SIGHUP, AutoResetServer);
#endif
}

/* Force connections to close and then exit on SIGTERM, SIGINT */

/* ARGSUSED */
SIGVAL
GiveUp(sig)
    int	sig;
{
    dispatchException |= DE_TERMINATE;
    isItTimeToYield = TRUE;
}

static void
AbortServer()
{
    fflush(stderr);
    if (CoreDump)
	abort();
    exit (1);
}

void
Error(str)
    char *str;
{
    perror(str);
}

void UseMsg()
{
    ErrorF("use: lbxproxy [:<display>] [option]\n");
#ifdef MEMBUG
    ErrorF("-alloc int             chance alloc should fail\n");
#endif
    ErrorF("-help                  prints message with these options\n");
    ErrorF("-display               specify address of LBX server\n");
    ErrorF("-motion #              allow # motion events in flight\n");
    ErrorF("-[terminate|reset]     terminate or reset after last client exits\n");
    ErrorF("                         (default is continue running)\n");
    ErrorF("-I                     ignore all remaining arguments\n");
    ErrorF("-reconnect             reset if server connection is broken\n");
    ErrorF("                         (default is to exit if connection is broken)\n");
    ErrorF("-nolbx                 disable LBX reencoding of X requests\n");
    ErrorF("-nocomp                disable stream compression\n");
    ErrorF("-nodelta               disable request deltas\n");
    ErrorF("-notags                disable tags\n");
    ErrorF("-nogfx                 disable graphics enhancements\n");
    ErrorF("-noimage               disable image compression\n");
    ErrorF("-nosquish              disable event squishing\n");
    ErrorF("-nointernsc            disable InternAtom short circuiting\n");
    ErrorF("-noatomsfile           disable atom control file\n");
    ErrorF("-atomsfile             override AtomControl file\n");
    ErrorF("-nowinattr             disable GetWindowAttributes/GetGeometry\n");
    ErrorF("                          grouping into one round trip\n");
    ErrorF("-nograbcmap            disable colormap grabbing\n");
    ErrorF("-norgbfile             disables color name to RGB resolution\n");
    ErrorF("-rgbfile <path>        path specifies an alternate RGB database\n");
    ErrorF("                          for color name to RGB resolution\n");
    ErrorF("-tagcachesize #        set tag cache size\n");
    ErrorF("-maxservers #          maximum number of servers to use\n");
    ErrorF("                          default is 20, but this is overrided\n");
    ErrorF("                          the following environment variable:\n");
    ErrorF("                          LBXPROXY_MAXSERVERS=<max servers>\n");
    ErrorF("-zlevel #              zlib compression level (1-9)\n");
    ErrorF("                          default is 9\n");
    ErrorF("                          1 = worst compression, fastest\n");
    ErrorF("                          9 = best compression, slowest\n");
    ErrorF("-compstats             report stream compression statistics\n");
    ErrorF("-nozeropad             don't zero out pad bytes in X requests\n");
    ErrorF("-cheaterrors           cheat on X protocol errors for better performance\n");
    ErrorF("-cheatevents           cheat on events and errors for better performance\n");
}

void
ShowHelpAndExit (status)
    int status;
{
    UseMsg ();
    exit (status);
}

static int
proxyProcessArgument (argc, argv, i)
    int argc;
    char    **argv;
    int i;
{
    if (strcmp (argv[i], "-debug") == 0)
    {
	if (++i < argc)
	    lbxDebug = atoi(argv[i]);
	else
	    ShowHelpAndExit (1);
	return 2;
    }
    if (strcmp (argv[i], "-cheaterrors") == 0)
    {
	protocolMode = PROTOCOL_MOST;
	return 1;
    }
    if (strcmp (argv[i], "-cheatevents") == 0)
    {
	protocolMode = PROTOCOL_POOR;
	return 1;
    }
    if (strcmp (argv[i], "-nolbx") == 0)
    {
	lbxUseLbx = FALSE;
	return 1;
    }
    if (strcmp (argv[i], "-nointernsc") == 0)
    {
	lbxDoAtomShortCircuiting = FALSE;
	return 1;
    }
    if (strcmp (argv[i], "-nocomp") == 0)
    {
	LbxNoComp();
	return 1;
    }
    if (strcmp (argv[i], "-nodelta") == 0)
    {
	LbxNoDelta();
	return 1;
    }
    if (strcmp (argv[i], "-notags") == 0)
    {
	lbxUseTags = FALSE;
	return 1;
    }
    if (strcmp (argv[i], "-nogfx") == 0)
    {
	lbxDoLbxGfx = FALSE;
	return 1;
    }
    if (strcmp (argv[i], "-noimage") == 0)
    {
	lbxCompressImages = FALSE;
	return 1;
    }
    if (strcmp (argv[i], "-nosquish") == 0)
    {
	lbxDoSquishing = FALSE;
	return 1;
    }
    if (strcmp (argv[i], "-nograbcmap") == 0)
    {
	lbxDoCmapGrabbing = FALSE;
	return 1;
    }
    if (strcmp (argv[i], "-reconnect") == 0)
    {
	reconnectAfterCloseServer = TRUE;
	return 1;
    }
    if (strcmp (argv[i], "-norgbfile") == 0)
    {
	rgbPath = NULL;
	return 1;
    }
    if (strcmp (argv[i], "-rgbfile") == 0)
    {
	if (++i < argc)
	    if (argv[i][0] == '-')
		ShowHelpAndExit (1);
	    else
		rgbPath = argv[i];
	else
	    ShowHelpAndExit (1);
	return 2;
    }
    if (strcmp (argv[i], "-nowinattr") == 0)
    {
	lbxWinAttr = FALSE;
	return 1;
    }
    if (strcmp (argv[i], "-noatomsfile") == 0)
    {
	atomsFile = NULL;
	return 1;
    }
    if (strcmp (argv[i], "-atomsfile") == 0)
    {
	if (++i < argc)
	{
	    if (argv[i][0] == '-')
		ShowHelpAndExit (1);
	    else
		atomsFile = argv[i];
	}
	else
	    ShowHelpAndExit (1);
	return 2;
    }
    if (strcmp (argv[i], "-tagcachesize") == 0)
    {
	if (++i < argc)
	    lbxTagCacheSize = atoi(argv[i]);
	else
	    ShowHelpAndExit (1);
	return 2;
    }
    if (strcmp (argv[i], "-maxservers") == 0)
    {
	if (++i < argc)
	    lbxMaxServers = atoi(argv[i]);
	else
	    ShowHelpAndExit (1);
	return 2;
    }
    if (strcmp (argv[i], "-motion") == 0)
    {
	if (++i < argc)
	    lbxMaxMotionEvents = atoi(argv[i]);
	else
	    ShowHelpAndExit (1);
	return 2;
    }
    if (strcmp (argv[i], "-zlevel") == 0)
    {
	if (++i < argc)
	{
	    zlevel = atoi(argv[i]);
	    if (zlevel < 1 || zlevel > 9)
		ShowHelpAndExit (1);
	}
	else
	    ShowHelpAndExit (1);
	return 2;
    }
    if (strcmp (argv[i], "-compstats") == 0)
    {
	compStats = TRUE;
	return 1;
    }
    if (strcmp (argv[i], "-nozeropad") == 0)
    {
	lbxZeroPad = FALSE;
	return 1;
    }
    return 0;
}

/*
 * This function parses the command line. Handles device-independent fields
 * and allows ddx to handle additional fields.  It is not allowed to modify
 * argc or any of the strings pointed to by argv.
 */
void
ProcessCommandLine ( argc, argv )
int	argc;
char	*argv[];

{
    int i, skip;
    char *env;

    /*
     * Some options may also be defined by environment variables.
     * However, if this is the case, the command line options will
     * take precedence so check the environment first.
     */
    if ((env = getenv ("LBXPROXY_MAXSERVERS")))
	lbxMaxServers = atoi (env);

    for ( i = 1; i < argc; i++ )
    {
	/* do proxy-specific stuff first */
        if((skip = proxyProcessArgument(argc, argv, i)))
	{
	    i += (skip - 1);
	}
	else if(argv[i][0] ==  ':')  
	{
	    /* initialize display */
	    display = argv[i];
	    display++;
	}
#ifdef MEMBUG
	else if ( strcmp( argv[i], "-alloc") == 0)
	{
	    if(++i < argc)
	        Memory_fail = atoi(argv[i]);
	    else
		ShowHelpAndExit (1);
	}
#endif
	else if ( strcmp( argv[i], "-display") == 0)
	{
	    if(++i < argc)
	        display_name = argv[i];
	    else
		ShowHelpAndExit (1);
	}
	else if ( strcmp( argv[i], "-core") == 0)
	    CoreDump = TRUE;
	else if ( strcmp( argv[i], "-help") == 0)
	    ShowHelpAndExit (0);
	else if ( strcmp( argv[i], "-pn") == 0)
	    PartialNetwork = TRUE;
	else if ( strcmp( argv[i], "-reset") == 0)
	{
	    if (terminateAfterLastClient)
		ShowHelpAndExit (1);

	    resetAfterLastClient = TRUE;
	}
	else if ( strcmp( argv[i], "-terminate") == 0)
	{
	    if (resetAfterLastClient)
		ShowHelpAndExit (1);

	    terminateAfterLastClient = TRUE;
	}
	else if ( strcmp( argv[i], "-I") == 0)
	{
	    /* ignore all remaining arguments */
	    break;
	}
	else if (strncmp (argv[i], "tty", 3) == 0)
	{
	    /* just in case any body is interested */
	    dev_tty_from_init = argv[i];
	}
 	else
	    ShowHelpAndExit (1);
    }
}

/* XALLOC -- X's internal memory allocator.  Why does it return unsigned
 * int * instead of the more common char *?  Well, if you read K&R you'll
 * see they say that alloc must return a pointer "suitable for conversion"
 * to whatever type you really want.  In a full-blown generic allocator
 * there's no way to solve the alignment problems without potentially
 * wasting lots of space.  But we have a more limited problem. We know
 * we're only ever returning pointers to structures which will have to
 * be long word aligned.  So we are making a stronger guarantee.  It might
 * have made sense to make Xalloc return char * to conform with people's
 * expectations of malloc, but this makes lint happier.
 */

unsigned long * 
Xalloc (amount)
    unsigned long amount;
{
    register pointer  ptr;
	
    if ((long)amount <= 0)
	return (unsigned long *)NULL;
    /* aligned extra on long word boundary */
    amount = (amount + 3) & ~3;
#ifdef MEMBUG
    if (!Must_have_memory && Memory_fail &&
	((random() % MEM_FAIL_SCALE) < Memory_fail))
	return (unsigned long *)NULL;
#endif
    if ((ptr = (pointer)malloc(amount)))
	return (unsigned long *)ptr;
    if (Must_have_memory)
	FatalError("Out of memory");
    return (unsigned long *)NULL;
}

/*****************
 * Xcalloc
 *****************/

unsigned long *
Xcalloc (amount)
    unsigned long   amount;
{
    unsigned long   *ret;

    ret = Xalloc (amount);
    if (ret)
	bzero ((char *) ret, (int) amount);
    return ret;
}

/*****************
 * Xrealloc
 *****************/

unsigned long *
Xrealloc (ptr, amount)
    register pointer ptr;
    unsigned long amount;
{
#ifdef MEMBUG
    if (!Must_have_memory && Memory_fail &&
	((random() % MEM_FAIL_SCALE) < Memory_fail))
	return (unsigned long *)NULL;
#endif
    if ((long)amount <= 0)
    {
	if (ptr && !amount)
	    free(ptr);
	return (unsigned long *)NULL;
    }
    amount = (amount + 3) & ~3;
    if (ptr)
        ptr = (pointer)realloc((char *)ptr, amount);
    else
	ptr = (pointer)malloc(amount);
    if (ptr)
        return (unsigned long *)ptr;
    if (Must_have_memory)
	FatalError("Out of memory");
    return (unsigned long *)NULL;
}
                    
/*****************
 *  Xfree
 *    calls free 
 *****************/    

void
Xfree(ptr)
    register pointer ptr;
{
    if (ptr)
	free((char *)ptr); 
}

void
OsInitAllocator ()
{
#ifdef MEMBUG
    static int	been_here;

    /* Check the memory system after each generation */
    if (been_here)
	CheckMemory ();
    else
	been_here = 1;
#endif
}

void
AuditF(const char * f, ...)
{
#ifdef notyet		/* ever ? */
    time_t tm;
    char *autime, *s;
    va_list args;

    if (*f != ' ')
    {
	time(&tm);
	autime = ctime(&tm);
	if (s = strchr(autime, '\n'))
	    *s = '\0';
	if (s = strrchr(argvGlobal[0], '/'))
	    s++;
	else
	    s = argvGlobal[0];
	ErrorF("AUDIT: %s: %d %s: ", autime, getpid(), s);
    }
    va_start(args, f);
    VErrorF(f, args);
    va_end(args);
#endif
}

void
FatalError(const char *f, ...)
{
    va_list args;
    ErrorF("\nFatal lbxproxy error: ");
    va_start(args, f);
    VErrorF(f, args);
    va_end(args);
    ErrorF("\n");
    AbortServer();
    /*NOTREACHED*/
}

static void
VErrorF(const char *f, va_list args)
{
    vfprintf(stderr, f, args);
}

void
ErrorF(const char * f, ...)
{
    va_list args;
    va_start(args, f);
    VErrorF(f, args);
    va_end(args);
}

char *
strnalloc(str, len)
    char       *str;
    int 	len;
{
    char       *t;

    t = (char *) Xalloc(len);
    if (!t)
	return (char *) 0;
    memcpy(t, str, len);
    return t;
}

/*
 * A general work queue.  Perform some task before the server
 * sleeps for input.
 */

typedef struct _WorkQueue {
    struct _WorkQueue *next;
    Bool        (*function) (
#if NeedNestedPrototypes
		ClientPtr	/* pClient */,
		pointer		/* closure */
#endif
);
    ClientPtr   client;
    pointer     closure;
}           WorkQueueRec;

WorkQueuePtr		workQueue;
static WorkQueuePtr	*workQueueLast = &workQueue;

/* ARGSUSED */
void
ProcessWorkQueue()
{
    WorkQueuePtr    q, n, p;

    p = NULL;
    /*
     * Scan the work queue once, calling each function.  Those
     * which return TRUE are removed from the queue, otherwise
     * they will be called again.  This must be reentrant with
     * QueueWorkProc, hence the crufty usage of variables.
     */
    for (q = workQueue; q; q = n)
    {
	if ((*q->function) (q->client, q->closure))
	{
	    /* remove q from the list */
	    n = q->next;    /* don't fetch until after func called */
	    if (p)
		p->next = n;
	    else
		workQueue = n;
	    xfree (q);
	}
	else
	{
	    n = q->next;    /* don't fetch until after func called */
	    p = q;
	}
    }
    if (p)
	workQueueLast = &p->next;
    else
    {
	workQueueLast = &workQueue;
    }
}

Bool
QueueWorkProc (function, client, closure)
    Bool	(*function)();
    ClientPtr	client;
    pointer	closure;
{
    WorkQueuePtr    q;

    q = (WorkQueuePtr) xalloc (sizeof *q);
    if (!q)
	return FALSE;
    q->function = function;
    q->client = client;
    q->closure = closure;
    q->next = NULL;
    *workQueueLast = q;
    workQueueLast = &q->next;
    return TRUE;
}

/*
 * Manage a queue of sleeping clients, awakening them
 * when requested, by using the OS functions IgnoreClient
 * and AttendClient.  Note that this *ignores* the troubles
 * with request data interleaving itself with events, but
 * we'll leave that until a later time.
 */

typedef struct _SleepQueue {
    struct _SleepQueue	*next;
    ClientPtr		client;
    Bool		(*function)();
    pointer		closure;
} SleepQueueRec, *SleepQueuePtr;

static SleepQueuePtr	sleepQueue = NULL;

Bool
ClientSleep (client, function, closure)
    ClientPtr	client;
    Bool	(*function)();
    pointer	closure;
{
    SleepQueuePtr   q;

    q = (SleepQueuePtr) xalloc (sizeof *q);
    if (!q)
	return FALSE;

    IgnoreClient (client);
    q->next = sleepQueue;
    q->client = client;
    q->function = function;
    q->closure = closure;
    sleepQueue = q;
    return TRUE;
}

Bool
ClientSignal (client)
    ClientPtr	client;
{
    SleepQueuePtr   q;

    for (q = sleepQueue; q; q = q->next)
	if (q->client == client)
	{
	    return QueueWorkProc (q->function, q->client, q->closure);
	}
    return FALSE;
}

void
ClientWakeup (client)
    ClientPtr	client;
{
    SleepQueuePtr   q, *prev;

    prev = &sleepQueue;
    while ((q = *prev))
    {
	if (q->client == client)
	{
	    *prev = q->next;
	    xfree (q);
	    if (!client->clientGone)
		AttendClient (client);
	    break;
	}
	prev = &q->next;
    }
}

Bool
ClientIsAsleep (client)
    ClientPtr	client;
{
    SleepQueuePtr   q;

    for (q = sleepQueue; q; q = q->next)
	if (q->client == client)
	    return TRUE;
    return FALSE;
}

#ifdef __UNIXOS2__
/* This code is duplicated from XLibInt.c, because the same problems with
 * the drive letter as in clients also exist in the server
 * Unfortunately the standalone servers don't link against libX11
 */

char *__XOS2RedirRoot(char *fname)
{
    /* This adds a further redirection by allowing the ProjectRoot
     * to be prepended by the content of the envvar X11ROOT.
     * This is for the purpose to move the whole X11 stuff to a different
     * disk drive.
     * The feature was added despite various environment variables
     * because not all file opens respect them.
     */
    static char redirname[300]; /* enough for long filenames */
    char *root;

    /* if name does not start with /, assume it is not root-based */
    if (fname==0 || !(fname[0]=='/' || fname[0]=='\\'))
	return fname;

    root = (char*)getenv("X11ROOT");
    if (root==0 || 
	(fname[1]==':' && isalpha(fname[0]) ||
        (strlen(fname)+strlen(root)+2) > 300))
	return fname;
    sprintf(redirname,"%s%s",root,fname);
    return redirname;
}
#endif





void
LBXReadAtomsFile (server)
    XServerPtr server;
{
    FILE *f;
    char buf[256], *p;
    int len;

    if (!atomsFile)
	return;

    while (server->atom_control_count)
	xfree(server->atom_control[--server->atom_control_count].name);
    xfree(server->atom_control);
    server->atom_control = NULL;
    min_keep_prop_size = DEF_KEEP_PROP_SIZE;

#ifdef __UNIXOS2__
    atomsFile = (char*)__XOS2RedirRoot(atomsFile);
#endif
    if (!(f = fopen (atomsFile, "r"))) {
	ErrorF ("Could not load atom control file: %s\n", atomsFile);
	return;
    }

    while (fgets (buf, 256, f))
	if (*buf != '!' && *buf != 0 && *buf != '\n')
	    server->atom_control_count++;

    if (!server->atom_control_count) {
	fclose(f);
	return;
    }

    server->atom_control = (AtomControlPtr) xalloc (server->atom_control_count *
					     sizeof(AtomControlRec));

    server->atom_control_count = 0;

    if (!server->atom_control) {
	fclose(f);
	return;
    }

    fseek (f, 0, 0);

    while (fgets (buf, 256, f))
    {
	if (*buf == '!' || *buf == 0 || *buf == '\n')
	    continue;

	len = strlen(buf);
	if (buf[len - 1] == '\n')
	    buf[--len] = 0;
	p = buf;
	if (*p == 'z') {
	    do {
		p++;
	    } while (*p == ' ' || *p == '\t');
	    min_keep_prop_size = atoi(p);
	    continue;
	}
	server->atom_control[server->atom_control_count].flags = 0;
	while (*p && *p != ' ' && *p != '\t') {
	    switch (*p) {
	    case 'i':
		server->atom_control[server->atom_control_count].flags 
				|= AtomPreInternFlag;
		break;
	    case 'n':
		server->atom_control[server->atom_control_count].flags 
				|= AtomNoCacheFlag;
		break;
	    case 'w':
		server->atom_control[server->atom_control_count].flags 
				|= AtomWMCacheFlag;
		break;
	    default:
		fprintf(stderr, "bad atom control: %c\n", *p);
		break;
	    }
	    p++;
	}
	while (*p == ' ' || *p == '\t')
	    p++;
	if (!*p)
	    continue;
	len = strlen(p);
	server->atom_control[server->atom_control_count].name = 
			(char *) xalloc(len + 1);
	if (server->atom_control[server->atom_control_count].name) {
	    server->atom_control[server->atom_control_count].len = len;
	    strcpy(server->atom_control[server->atom_control_count].name, p);
	    server->atom_control_count++;
	}
    }

    fclose(f);
}
