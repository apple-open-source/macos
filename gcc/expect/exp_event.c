/* exp_event.c - event interface for Expect

Written by: Don Libes, NIST, 2/6/90

Design and implementation of this program was paid for by U.S. tax
dollars.  Therefore it is public domain.  However, the author and NIST
would appreciate credit if this program or parts of it are used.

*/

/* Notes:
I'm only a little worried because Tk does not check for errno == EBADF
after calling select.  I imagine that if the user passes in a bad file
descriptor, we'll never get called back, and thus, we'll hang forever
- it would be better to at least issue a diagnostic to the user.

Another possible problem: Tk does not do file callbacks round-robin.

Another possible problem: Calling Create/DeleteFileHandler
before/after every Tcl_Eval... in expect/interact could be very
expensive.

*/


#include "expect_cf.h"
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_PTYTRAP
#  include <sys/ptyio.h>
#endif

#include "tcl.h"
#include "exp_prog.h"
#include "exp_command.h"	/* for struct exp_f defs */
#include "exp_event.h"

/* Tcl_DoOneEvent will call our filehandler which will set the following */
/* vars enabling us to know where and what kind of I/O we can do */
/*#define EXP_SPAWN_ID_BAD	-1*/
/*#define EXP_SPAWN_ID_TIMEOUT	-2*/	/* really indicates a timeout */

static int ready_fd = EXP_SPAWN_ID_BAD;
static int ready_mask;
static int default_mask = TCL_READABLE | TCL_EXCEPTION;


void
exp_event_disarm(fd)
int fd;
{
#if TCL_MAJOR_VERSION < 8
	Tcl_DeleteFileHandler(exp_fs[fd].Master);
#else
	Tcl_DeleteFileHandler(fd);
#endif

	/* remember that filehandler has been disabled so that */
	/* it can be turned on for fg expect's as well as bg */
	exp_fs[fd].fg_armed = FALSE;
}

void
exp_event_disarm_fast(fd,filehandler)
int fd;
Tcl_FileProc *filehandler;
{
	/* Temporarily delete the filehandler by assigning it a mask */
	/* that permits no events! */
	/* This reduces the calls to malloc/free inside Tcl_...FileHandler */
	/* Tk insists on having a valid proc here even though it isn't used */
#if TCL_MAJOR_VERSION < 8
	Tcl_CreateFileHandler(exp_fs[fd].Master,0,filehandler,(ClientData)0);
#else
	Tcl_CreateFileHandler(fd,0,filehandler,(ClientData)0);
#endif

	/* remember that filehandler has been disabled so that */
	/* it can be turned on for fg expect's as well as bg */
	exp_fs[fd].fg_armed = FALSE;
}

static void
exp_arm_background_filehandler_force(m)
int m;
{
#if TCL_MAJOR_VERSION < 8
	Tcl_CreateFileHandler(exp_fs[m].Master,
#else
	Tcl_CreateFileHandler(m,
#endif
		TCL_READABLE|TCL_EXCEPTION,
		exp_background_filehandler,
		(ClientData)(exp_fs[m].fd_ptr));

	exp_fs[m].bg_status = armed;
}

void
exp_arm_background_filehandler(m)
int m;
{
	switch (exp_fs[m].bg_status) {
	case unarmed:
		exp_arm_background_filehandler_force(m);
		break;
	case disarm_req_while_blocked:
		exp_fs[m].bg_status = blocked;	/* forget request */
		break;
	case armed:
	case blocked:
		/* do nothing */
		break;
	}
}

void
exp_disarm_background_filehandler(m)
int m;
{
	switch (exp_fs[m].bg_status) {
	case blocked:
		exp_fs[m].bg_status = disarm_req_while_blocked;
		break;
	case armed:
		exp_fs[m].bg_status = unarmed;
		exp_event_disarm(m);
		break;
	case disarm_req_while_blocked:
	case unarmed:
		/* do nothing */
		break;
	}
}

/* ignore block status and forcibly disarm handler - called from exp_close. */
/* After exp_close returns, we will not have an opportunity to disarm */
/* because the fd will be invalid, so we force it here. */
void
exp_disarm_background_filehandler_force(m)
int m;
{
	switch (exp_fs[m].bg_status) {
	case blocked:
	case disarm_req_while_blocked:
	case armed:
		exp_fs[m].bg_status = unarmed;
		exp_event_disarm(m);
		break;
	case unarmed:
		/* do nothing */
		break;
	}
}

/* this can only be called at the end of the bg handler in which */
/* case we know the status is some kind of "blocked" */
void
exp_unblock_background_filehandler(m)
int m;
{
	switch (exp_fs[m].bg_status) {
	case blocked:
		exp_arm_background_filehandler_force(m);
		break;
	case disarm_req_while_blocked:
		exp_disarm_background_filehandler_force(m);
		break;
	case armed:
	case unarmed:
		/* Not handled, FIXME? */
		break;
	}
}

/* this can only be called at the beginning of the bg handler in which */
/* case we know the status must be "armed" */
void
exp_block_background_filehandler(m)
int m;
{
	exp_fs[m].bg_status = blocked;
	exp_event_disarm_fast(m,exp_background_filehandler);
}


/*ARGSUSED*/
static void
exp_timehandler(clientData)
ClientData clientData;
{
	*(int *)clientData = TRUE;	
}

static void exp_filehandler(clientData,mask)
ClientData clientData;
int mask;
{
	/* if input appears, record the fd on which it appeared */

	ready_fd = *(int *)clientData;
	ready_mask = mask;
	exp_event_disarm_fast(ready_fd,exp_filehandler);

#if 0
	if (ready_fd == *(int *)clientData) {
		/* if input appears from an fd which we've already heard */
		/* forcibly tell it to shut up.  We could also shut up */
		/* every instance, but it is more efficient to leave the */
		/* fd enabled with the belief that we may rearm soon enough */
		/* anyway. */

		exp_event_disarm_fast(ready_fd,exp_filehandler);
	} else {
		ready_fd = *(int *)clientData;
		ready_mask = mask;
	}
#endif
}

/* returns status, one of EOF, TIMEOUT, ERROR or DATA */
/* can now return RECONFIGURE, too */
/*ARGSUSED*/
int exp_get_next_event(interp,masters, n,master_out,timeout,key)
Tcl_Interp *interp;
int *masters;
int n;			/* # of masters */
int *master_out;	/* 1st ready master, not set if none */
int timeout;		/* seconds */
int key;
{
	static rr = 0;	/* round robin ptr */
	int i;	/* index into in-array */
#ifdef HAVE_PTYTRAP
	struct request_info ioctl_info;
#endif

	int old_configure_count = exp_configure_count;

	int timer_created = FALSE;
	int timer_fired = FALSE;
	Tcl_TimerToken timetoken;/* handle to Tcl timehandler descriptor */

	for (;;) {
		int m;
		struct exp_f *f;

		/* if anything has been touched by someone else, report that */
		/* an event has been received */

		for (i=0;i<n;i++) {
			rr++;
			if (rr >= n) rr = 0;

			m = masters[rr];
			f = exp_fs + m;

			if (f->key != key) {
				f->key = key;
				f->force_read = FALSE;
				*master_out = m;
				return(EXP_DATA_OLD);
			} else if ((!f->force_read) && (f->size != 0)) {
				*master_out = m;
				return(EXP_DATA_OLD);
			}
		}

		if (!timer_created) {
			if (timeout >= 0) {
				timetoken = Tcl_CreateTimerHandler(1000*timeout,
						exp_timehandler,
						(ClientData)&timer_fired);
				timer_created = TRUE;
			}
		}

		for (;;) {
			int j;

			/* make sure that all fds that should be armed are */
			for (j=0;j<n;j++) {
				int k = masters[j];

				if (!exp_fs[k].fg_armed) {
					Tcl_CreateFileHandler(
#if TCL_MAJOR_VERSION < 8
					     exp_fs[k].Master,
#else
					     k,
#endif
					     default_mask,
					     exp_filehandler,
					     (ClientData)exp_fs[k].fd_ptr);
					exp_fs[k].fg_armed = TRUE;
				}
			}

			Tcl_DoOneEvent(0);	/* do any event */

			if (timer_fired) return(EXP_TIMEOUT);

			if (old_configure_count != exp_configure_count) {
				if (timer_created) Tcl_DeleteTimerHandler(timetoken);
				return EXP_RECONFIGURE;
			}

			if (ready_fd == EXP_SPAWN_ID_BAD) continue;

			/* if it was from something we're not looking for at */
			/* the moment, ignore it */
			for (j=0;j<n;j++) {
				if (ready_fd == masters[j]) goto found;
			}

			/* not found */
			exp_event_disarm_fast(ready_fd,exp_filehandler);
			ready_fd = EXP_SPAWN_ID_BAD;
			continue;
		found:
			*master_out = ready_fd;
			ready_fd = EXP_SPAWN_ID_BAD;

			/* this test should be redundant but SunOS */
			/* raises both READABLE and EXCEPTION (for no */
			/* apparent reason) when selecting on a plain file */
			if (ready_mask & TCL_READABLE) {
				if (timer_created) Tcl_DeleteTimerHandler(timetoken);
				return EXP_DATA_NEW;
			}

			/* ready_mask must contain TCL_EXCEPTION */
#ifndef HAVE_PTYTRAP
			if (timer_created) Tcl_DeleteTimerHandler(timetoken);
			return(EXP_EOF);
#else
			if (ioctl(*master_out,TIOCREQCHECK,&ioctl_info) < 0) {
				if (timer_created)
					Tcl_DeleteTimerHandler(timetoken);
				exp_debuglog("ioctl error on TIOCREQCHECK: %s", Tcl_PosixError(interp));
				return(EXP_TCLERROR);
			}
			if (ioctl_info.request == TIOCCLOSE) {
				if (timer_created)
					Tcl_DeleteTimerHandler(timetoken);
				return(EXP_EOF);
			}
			if (ioctl(*master_out, TIOCREQSET, &ioctl_info) < 0) {
				exp_debuglog("ioctl error on TIOCREQSET after ioctl or open on slave: %s", Tcl_ErrnoMsg(errno));
			}
			/* presumably, we trapped an open here */
			continue;
#endif /* !HAVE_PTYTRAP */
		}
	}
}

/* Having been told there was an event for a specific fd, get it */
/* returns status, one of EOF, TIMEOUT, ERROR or DATA */
/*ARGSUSED*/
int
exp_get_next_event_info(interp,fd,ready_mask)
Tcl_Interp *interp;
int fd;
int ready_mask;
{
#ifdef HAVE_PTYTRAP
	struct request_info ioctl_info;
#endif

	if (ready_mask & TCL_READABLE) return EXP_DATA_NEW;

	/* ready_mask must contain TCL_EXCEPTION */

#ifndef HAVE_PTYTRAP
	return(EXP_EOF);
#else
	if (ioctl(fd,TIOCREQCHECK,&ioctl_info) < 0) {
		exp_debuglog("ioctl error on TIOCREQCHECK: %s",
				Tcl_PosixError(interp));
		return(EXP_TCLERROR);
	}
	if (ioctl_info.request == TIOCCLOSE) {
		return(EXP_EOF);
	}
	if (ioctl(fd, TIOCREQSET, &ioctl_info) < 0) {
		exp_debuglog("ioctl error on TIOCREQSET after ioctl or open on slave: %s", Tcl_ErrnoMsg(errno));
	}
	/* presumably, we trapped an open here */
	/* call it an error for lack of anything more descriptive */
	/* it will be thrown away by caller anyway */
	return EXP_TCLERROR;
#endif
}

/*ARGSUSED*/
int	/* returns TCL_XXX */
exp_dsleep(interp,sec)
Tcl_Interp *interp;
double sec;
{
	int timer_fired = FALSE;

	Tcl_CreateTimerHandler((int)(sec*1000),exp_timehandler,(ClientData)&timer_fired);

	while (1) {
		Tcl_DoOneEvent(0);
		if (timer_fired) return TCL_OK;

		if (ready_fd == EXP_SPAWN_ID_BAD) continue;

		exp_event_disarm_fast(ready_fd,exp_filehandler);
		ready_fd = EXP_SPAWN_ID_BAD;
	}
}

#if 0
/*ARGSUSED*/
int	/* returns TCL_XXX */
exp_usleep(interp,usec)
Tcl_Interp *interp;
long usec;
{
	int timer_fired = FALSE;

	Tcl_CreateTimerHandler(usec/1000,exp_timehandler,(ClientData)&timer_fired);

	while (1) {
		Tcl_DoOneEvent(0);
		if (timer_fired) return TCL_OK;

		if (ready_fd == EXP_SPAWN_ID_BAD) continue;

		exp_event_disarm_fast(ready_fd,exp_filehandler);
		ready_fd = EXP_SPAWN_ID_BAD;
	}
}
#endif

static char destroy_cmd[] = "destroy .";

static void
exp_event_exit_real(interp)
Tcl_Interp *interp;
{
	Tcl_Eval(interp,destroy_cmd);
}

/* set things up for later calls to event handler */
void
exp_init_event()
{
	exp_event_exit = exp_event_exit_real;
}

#ifdef __CYGWIN32__
#ifndef CYGWIN_ALTTCL

/* The Tcl_CreateFileHandler call is only defined on Unix.  We provide
   our own implementation here that works on cygwin32.  */

#include <windows.h>
#include <sys/socket.h>
#include <tclInt.h>

#if TCL_MAJOR_VERSION < 7
# error not implemented
#endif

static void pipe_setup _ANSI_ARGS_((ClientData, int));
static void pipe_check _ANSI_ARGS_((ClientData, int));
static void pipe_exit _ANSI_ARGS_((ClientData));
static int pipe_close _ANSI_ARGS_((ClientData, Tcl_Interp *));
static int pipe_input _ANSI_ARGS_((ClientData, char *, int, int *));
static int pipe_output _ANSI_ARGS_((ClientData, char *, int, int *));
static void pipe_watch _ANSI_ARGS_((ClientData, int));
static int pipe_get_handle _ANSI_ARGS_((ClientData, int, ClientData *));
static int pipe_event _ANSI_ARGS_((Tcl_Event *, int));

/* The pipe channel interface.  */

static Tcl_ChannelType pipe_channel = {
    "expect_pipe",
    NULL, /* block */
    pipe_close,
    pipe_input,
    pipe_output,
    NULL, /* seek */
    NULL, /* set option */
    NULL, /* get option */
    pipe_watch,
    pipe_get_handle
};

/* The structure we use to represent a pipe channel.  */

struct pipe_info {
    struct pipe_info *next;	/* Next pipe.  */
    Tcl_Channel channel;	/* The Tcl channel.  */
    int fd;			/* cygwin32 file descriptor.  */
    int watch_mask;		/* Events that should be reported.  */
    int flags;			/* State flags; see below.  */
    HANDLE flags_mutex;		/* Mutex to control access to flags.  */
    HANDLE mutex;		/* Mutex to control access to pipe.  */
    HANDLE try_read;		/* Event to tell thread to try a read.  */
    HANDLE pthread;		/* Handle of thread inspecting the pipe. */
};

/* Values that can appear in the flags field of a pipe_info structure.  */

#define PIPE_PENDING	(1 << 0)	/* Message pending.  */
#define PIPE_READABLE	(1 << 1)	/* Pipe is readable.  */
#define PIPE_CLOSED	(1 << 2)	/* Pipe is closed.  */
#define PIPE_HAS_THREAD	(1 << 3)	/* A thread is running.  */

/* A pipe event structure.  */

struct pipe_event {
    Tcl_Event header;		/* Standard Tcl event header.  */
    struct pipe_info *pipe;	/* Pipe information.  */
};

/* Whether we've initialized the pipe code.  */

static int pipe_initialized;

/* The list of pipes.  */

static struct pipe_info *pipes;

/* A hidden window we use for pipe events.  */

static HWND pipe_window;

/* A message we use for pipe events.  */

#define PIPE_MESSAGE (WM_USER + 1)

/* Get the flags for a pipe.  */

static int
pipe_get_flags (pipe)
struct pipe_info *pipe;
{
	int flags;

	WaitForSingleObject (pipe->flags_mutex, INFINITE);
	flags = pipe->flags;
	ReleaseMutex (pipe->flags_mutex);
	return flags;
}

/* Set a flag for a pipe.  */

static void
pipe_set_flag (pipe, flag)
struct pipe_info *pipe;
int flag;
{
	WaitForSingleObject (pipe->flags_mutex, INFINITE);
	pipe->flags |= flag;
	ReleaseMutex (pipe->flags_mutex);
}

/* Reset a flag for a pipe.  */

static void
pipe_reset_flag (pipe, flag)
struct pipe_info *pipe;
int flag;
{
	WaitForSingleObject (pipe->flags_mutex, INFINITE);
	pipe->flags &= ~ flag;
	ReleaseMutex (pipe->flags_mutex);
}

/* This function runs in a separate thread.  When requested, it sends
   a message if there is something to read from the pipe.  FIXME: I'm
   not sure that this thread will ever be killed off at present.  */

static DWORD
pipe_thread (arg)
LPVOID arg;
{
	struct pipe_info *pipe = (struct pipe_info *) arg;
	HANDLE handle = get_osfhandle(pipe->fd);
	struct timeval timeout;

	while (1) {
		int n, tba;
		fd_set r, x;

		/* time out in case this thread was "forgotten" */
		if (WaitForSingleObject (pipe->try_read, 10000) == WAIT_TIMEOUT)
		  {	
		    n = PeekNamedPipe(handle, NULL, 0, NULL, &tba, NULL);
		    if (n == 0)
		      break; /* pipe closed? */
		  }

		if (pipe_get_flags (pipe) & PIPE_CLOSED) {
			break;
		}

		/* We use a loop periodically trying PeekNamedPipe.
                   This is inefficient, but unfortunately Windows
                   doesn't have any asynchronous mechanism to read
                   from a pipe.  */

		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
		FD_ZERO (&r);
		FD_SET (pipe->fd, &r);
		FD_ZERO (&x);
		FD_SET (pipe->fd, &x);
		if ((n = select (pipe->fd + 1, &r, NULL, &x, &timeout)) <= 0 ||
		    FD_ISSET(pipe->fd, &x))
		  /* pipe_set_flag (pipe, PIPE_CLOSED)*/;

		/* Something can be read from the pipe.  */
		pipe_set_flag (pipe, PIPE_READABLE);

		if (pipe_get_flags (pipe) & PIPE_CLOSED) {
			break;
		}

		/* Post a message to wake up the event loop.  */
		PostMessage (pipe_window, PIPE_MESSAGE, 0, (LPARAM) pipe);
		if (n < 0 || FD_ISSET (pipe->fd, &x))
			break;
	}

	/* The pipe is closed.  */

	CloseHandle (pipe->flags_mutex); pipe->flags_mutex = NULL;
	CloseHandle (pipe->try_read); pipe->try_read = NULL;
	pipe_reset_flag (pipe, PIPE_HAS_THREAD);
	return 0;
}

/* This function is called when pipe_thread posts a message.  */

static LRESULT CALLBACK
pipe_proc (hwnd, message, wParam, lParam)
HWND hwnd;
UINT message;
WPARAM wParam;
LPARAM lParam;
{
	if (message != PIPE_MESSAGE) {
		return DefWindowProc (hwnd, message, wParam, lParam);
	}

	/* This function really only exists to wake up the main Tcl
           event loop.  We don't actually have to do anything.  */

	return 0;
}

/* Initialize the pipe channel.  */

static void
pipe_init ()
{
	WNDCLASS class;

	pipe_initialized = 1;

	Tcl_CreateEventSource (pipe_setup, pipe_check, NULL);
	Tcl_CreateExitHandler (pipe_exit, NULL);

	/* Create a hidden window for asynchronous notification.  */

	memset (&class, 0, sizeof class);
	class.hInstance = GetModuleHandle (NULL);
	class.lpszClassName = "expect_pipe";
	class.lpfnWndProc = pipe_proc;

	if (! RegisterClass (&class)) {
		exp_errorlog ("RegisterClass failed: %d\n", GetLastError ());
		exit (-1);
	}

	pipe_window = CreateWindow ("expect_pipe", "expect_pipe",
				    WS_TILED, 0, 0, 0, 0, NULL, NULL,
				    class.hInstance, NULL);
	if (pipe_window == NULL) {
		exp_errorlog ("CreateWindow failed: %d\n", GetLastError ());
		exit (-1);
	}
}

/* Clean up the pipe channel when we exit.  */

static void
pipe_exit (cd)
ClientData cd;
{
	Tcl_DeleteEventSource (pipe_setup, pipe_check, NULL);
	UnregisterClass ("expect_pipe", GetModuleHandle (NULL));
	DestroyWindow (pipe_window);
	pipe_initialized = 0;
}

/* Set up for a pipe event.  */

static void
pipe_setup (cd, flags)
ClientData cd;
int flags;
{
	struct pipe_info *p;
	Tcl_Time zero_time = { 0, 0 };

	if (! (flags & TCL_FILE_EVENTS)) {
		return;
	}

	/* See if there is a watched pipe for which we already have an
           event.  If there is, set the maximum block time to 0.  */

	for (p = pipes; p != NULL; p = p->next) {
		if ((p->watch_mask &~ TCL_READABLE)
		    || ((p->watch_mask & TCL_READABLE)
			&& ((pipe_get_flags (p) & PIPE_HAS_THREAD) == 0
			    || (pipe_get_flags (p) & PIPE_READABLE)))) {
			Tcl_SetMaxBlockTime (&zero_time);
			break;
		} else if (p->watch_mask & TCL_READABLE) {
			/* Tell the thread to try reads and let us
                           know when it is done.  */
			SetEvent (p->try_read);
		}
	}
}

/* Check pipes for events.  */

static void
pipe_check (cd, flags)
ClientData cd;
int flags;
{
	struct pipe_info *p;

	if (! (flags & TCL_FILE_EVENTS)) {
		return;
	}

	/* Queue events for any watched pipes that don't already have
           events queued.  */

	for (p = pipes; p != NULL; p = p->next) {
		if (((p->watch_mask &~ TCL_READABLE)
		     || ((p->watch_mask & TCL_READABLE)
			 && ((pipe_get_flags (p) & PIPE_HAS_THREAD) == 0
			     || (pipe_get_flags (p) & PIPE_READABLE))))
		    && ! (pipe_get_flags (p) & PIPE_PENDING)) {
			struct pipe_event *ev;

			pipe_set_flag (p, PIPE_PENDING);
			ev = (struct pipe_event *) Tcl_Alloc (sizeof *ev);
			ev->header.proc = pipe_event;
			ev->pipe = p;
			Tcl_QueueEvent ((Tcl_Event *) ev, TCL_QUEUE_TAIL);
		}
	}
}

/* Handle closing a pipe.  This is probably never called at present.  */

static int
pipe_close (cd, interp)
ClientData cd;
Tcl_Interp *interp;
{
	struct pipe_info *p = (struct pipe_info *) cd;
	struct pipe_info **pp;

	for (pp = &pipes; *pp != NULL; pp = &(*pp)->next) {
		if (*pp == p) {
			*pp = p->next;
			break;
		}
	}

	/* FIXME: How should we handle closing the handle?  At
           present, this code will only work correctly if the handle
           is closed before this code is called; otherwise, we may
           wait forever for the thread.  */

	if (pipe_get_flags (p) & PIPE_HAS_THREAD) {
		close (p->fd);
		pipe_set_flag (p, PIPE_CLOSED);
		(void) SetEvent (p->try_read);
		WaitForSingleObject (p->pthread, 10000);
		CloseHandle (p->pthread);
	} else {
		CloseHandle (p->flags_mutex);
	}
	Tcl_Free ((char *) p);

	return 0;
}

/* Handle reading from a pipe.  This will never be called.  */

static int
pipe_input (cd, buf, size, error)
ClientData cd;
char *buf;
int size;
int *error;
{
	exp_errorlog ("pipe_input called");
	exit (-1);
}

/* Handle writing to a pipe.  This will never be called.  */

static int
pipe_output (cd, buf, size, error)
ClientData cd;
char *buf;
int size;
int *error;
{
	exp_errorlog ("pipe_output called");
	exit (-1);
}

/* Handle a pipe event.  This is called when a pipe event created by
   pipe_check reaches the head of the Tcl queue.  */

static int
pipe_event (ev, flags)
Tcl_Event *ev;
int flags;
{
	struct pipe_event *pev = (struct pipe_event *) ev;
	struct pipe_info *p;
	int mask;

	if (! (flags & TCL_FILE_EVENTS)) {
		return 0;
	}

	/* Make sure the pipe is still being watched.  */
	for (p = pipes; p != NULL; p = p->next) {
		if (p == pev->pipe) {
			pipe_reset_flag (p, PIPE_PENDING);
			break;
		}
	}

	if (p == NULL) {
		return 1;
	}

	if (pipe_get_flags (p) & PIPE_HAS_THREAD) {
		mask = TCL_WRITABLE;
		if (pipe_get_flags (p) & PIPE_READABLE) {
			mask |= TCL_READABLE;
		}
	} else {
		mask = TCL_WRITABLE | TCL_READABLE;
	}

	/* Tell the channel about any events.  */

	Tcl_NotifyChannel (p->channel, p->watch_mask & mask);

	return 1;
}

/* Set up to watch a pipe.  */

static void
pipe_watch (cd, mask)
ClientData cd;
int mask;
{
	struct pipe_info *p = (struct pipe_info *) cd;
	int old_mask;

	old_mask = p->watch_mask;
	p->watch_mask = mask & (TCL_READABLE | TCL_WRITABLE);
	if (p->watch_mask != 0) {
		Tcl_Time zero_time = { 0, 0 };

		if ((p->watch_mask & TCL_READABLE) != 0
		    && (pipe_get_flags (p) & PIPE_HAS_THREAD) == 0) {
			HANDLE thread;
			DWORD tid;

			p->try_read = CreateEvent (NULL, FALSE, FALSE,
						      NULL);
			pipe_set_flag (p, PIPE_HAS_THREAD);
			p->pthread = CreateThread (NULL, 0, pipe_thread,
						   p, 0, &tid);
			/* CYGNUS LOCAL: plug a handle leak - dj */
			if (!p->pthread)
			  {
			    fprintf(stderr, "Error: cannot create pipe thread, error=%d\n", GetLastError());
			    exit(1);
			  }

			CloseHandle(p->pthread);
		}

		if (old_mask == 0) {
			p->next = pipes;
			pipes = p;
		}

		Tcl_SetMaxBlockTime (&zero_time);
	} else {
		if (old_mask != 0) {
			struct pipe_info **pp;

			for (pp = &pipes; *pp != NULL; pp = &(*pp)->next) {
				if (*pp == p) {
					*pp = p->next;
					break;
				}
			}
		}
	}
}

/* Get the handle of a pipe.  */

static int
pipe_get_handle (cd, dir, handle_ptr)
ClientData cd;
int dir;
ClientData *handle_ptr;
{
	struct pipe_info *p = (struct pipe_info *) cd;

	*handle_ptr = (ClientData *)p->fd;
	return TCL_OK;
}

/* Make a pipe channel.  */

static Tcl_Channel
make_pipe_channel (fd, handle)
int fd;
HANDLE handle;
{
	Tcl_Channel chan;
	struct pipe_info *p;
	char namebuf[30];

	if (! pipe_initialized) {
		pipe_init ();
	}

	p = (struct pipe_info *) Tcl_Alloc (sizeof *p);

	p->next = NULL;
	p->fd = fd;
	p->watch_mask = 0;
	p->flags = 0;
	p->flags_mutex = CreateMutex (NULL, FALSE, NULL);
	p->try_read = NULL;

	sprintf (namebuf, "expect_pipe%d", handle);
	p->channel = Tcl_CreateChannel (&pipe_channel, namebuf,
					(ClientData) p,
					TCL_READABLE | TCL_WRITABLE);

	Tcl_SetChannelOption ((Tcl_Interp *) NULL, p->channel,
			      "-translation", "binary");

	return p->channel;
}

/* This is called when we read from a file descriptor.  If that file
   descriptor turns out to be a pipe, we turn off the PIPE_READABLE
   flag.  If we didn't do this, then every time we entered the Tcl
   event loop we would think the pipe was readable.  If we read the
   pipe using Tcl channel functions, we wouldn't have this problem.  */

void
exp_reading_from_descriptor (fd)
int fd;
{
	struct pipe_info *p;

	for (p = pipes; p != NULL; p = p->next) {
		if (p->fd == fd) {
			pipe_reset_flag (p, PIPE_READABLE);
			break;
		}
	}
}

/* Implement Tcl_CreateFileHandler for cygwin32.   */

void
Tcl_CreateFileHandler (fd, mask, proc, cd)
int fd;
int mask;
Tcl_FileProc *proc;
ClientData cd;
{
	if (exp_fs[fd].channel == NULL) {
		HANDLE handle;
		Tcl_Channel chan;
		struct sockaddr sa;
		int salen;

		/* Tcl can handle channel events on Windows for
                   sockets and regular files.  For pipes we defines
                   our own channel type.  FIXME: The channels we
                   create here are never deleted.  */

		handle = (HANDLE) get_osfhandle (fd);
		if (handle == (HANDLE) -1)
			abort ();

		chan = NULL;

		salen = sizeof sa;
		if (getsockname (fd, &sa, &salen) == 0)
			chan = Tcl_MakeTcpClientChannel ((ClientData) handle);
		else if (GetFileType (handle) != FILE_TYPE_PIPE)
			chan = Tcl_MakeFileChannel ((ClientData) fd,
						    (TCL_READABLE
						     | TCL_WRITABLE));
		else {
			/* We assume that we can always write to a
                           pipe.  */
			if ((mask & TCL_READABLE) == 0)
				chan = Tcl_MakeFileChannel ((ClientData) fd,
							    mask);
			else
				chan = make_pipe_channel (fd, handle);
		}

		if (chan == NULL)
			abort ();

		exp_fs[fd].channel = chan;
	}

	if (exp_fs[fd].fileproc != NULL)
		Tcl_DeleteChannelHandler (exp_fs[fd].channel,
					  exp_fs[fd].fileproc,
					  exp_fs[fd].procdata);

	Tcl_CreateChannelHandler (exp_fs[fd].channel, mask, proc, cd);
	exp_fs[fd].fileproc = proc;
	exp_fs[fd].procdata = cd;
}

/* Implement Tcl_DeleteFileHandler for cygwin32.   */

void
Tcl_DeleteFileHandler (fd)
int fd;
{
	if (exp_fs[fd].channel != NULL && exp_fs[fd].fileproc != NULL) {
		Tcl_DeleteChannelHandler (exp_fs[fd].channel,
					  exp_fs[fd].fileproc,
					  exp_fs[fd].procdata);
		exp_fs[fd].fileproc = NULL;
	}
}

#endif /* CYGWIN_ALTTCL */
#endif /* __CYGWIN32__ */
