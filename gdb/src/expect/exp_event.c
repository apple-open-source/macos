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
