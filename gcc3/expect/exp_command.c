/* exp_command.c - the bulk of the Expect commands

Written by: Don Libes, NIST, 2/6/90

Design and implementation of this program was paid for by U.S. tax
dollars.  Therefore it is public domain.  However, the author and NIST
would appreciate credit if this program or parts of it are used.

*/

#include "expect_cf.h"

#include <stdio.h>
#include <sys/types.h>
/*#include <sys/time.h> seems to not be present on SVR3 systems */
/* and it's not used anyway as far as I can tell */

/* AIX insists that stropts.h be included before ioctl.h, because both */
/* define _IO but only ioctl.h checks first.  Oddly, they seem to be */
/* defined differently! */
#ifdef HAVE_STROPTS_H
#  include <sys/stropts.h>
#endif
#include <sys/ioctl.h>

#ifdef HAVE_SYS_FCNTL_H
#  include <sys/fcntl.h>
#else
#  include <fcntl.h>
#endif
#include <sys/file.h>
#include "exp_tty.h"

#ifdef HAVE_SYS_WAIT_H
  /* ISC doesn't def WNOHANG unless _POSIX_SOURCE is def'ed */
# ifdef WNOHANG_REQUIRES_POSIX_SOURCE
#  define _POSIX_SOURCE
# endif
# include <sys/wait.h>
# ifdef WNOHANG_REQUIRES_POSIX_SOURCE
#  undef _POSIX_SOURCE
# endif
#endif

#include <errno.h>
#include <signal.h>

#if defined(SIGCLD) && !defined(SIGCHLD)
#define SIGCHLD SIGCLD
#endif

/* Use _NSIG if NSIG not present */
#ifndef NSIG
#ifdef _NSIG
#define NSIG _NSIG
#endif
#endif

#ifdef HAVE_PTYTRAP
#include <sys/ptyio.h>
#endif

#ifdef CRAY
# ifndef TCSETCTTY
#  if defined(HAVE_TERMIOS)
#   include <termios.h>
#  else
#   include <termio.h>
#  endif
# endif
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <math.h>		/* for log/pow computation in send -h */
#include <ctype.h>		/* all this for ispunct! */

#include "tclInt.h"		/* need OpenFile */
/*#include <varargs.h>		tclInt.h drags in varargs.h.  Since Pyramid */
/*				objects to including varargs.h twice, just */
/*				omit this one. */

#include "tcl.h"
#include "string.h"
#include "expect_tcl.h"
#include "exp_rename.h"
#include "exp_prog.h"
#include "exp_command.h"
#include "exp_log.h"
#include "exp_event.h"
#include "exp_pty.h"
#ifdef TCL_DEBUGGER
#include "Dbg.h"
#endif

#define SPAWN_ID_VARNAME "spawn_id"

int getptymaster();
int getptyslave();

int exp_forked = FALSE;		/* whether we are child process */

/* the following are just reserved addresses, to be used as ClientData */
/* args to be used to tell commands how they were called. */
/* The actual values won't be used, only the addresses, but I give them */
/* values out of my irrational fear the compiler might collapse them all. */
static int sendCD_error = 2;	/* called as send_error */
static int sendCD_user = 3;	/* called as send_user */
static int sendCD_proc = 4;	/* called as send or send_spawn */
static int sendCD_tty = 6;	/* called as send_tty */

struct exp_f *exp_fs = 0;		/* process array (indexed by spawn_id's) */
int exp_fd_max = -1;		/* highest fd */

/*
 * expect_key is just a source for generating a unique stamp.  As each
 * expect/interact command begins, it generates a new key and marks all
 * the spawn ids of interest with it.  Then, if someone comes along and
 * marks them with yet a newer key, the old command will recognize this
 * reexamine the state of the spawned process.
 */
int expect_key = 0;

/*
 * exp_configure_count is incremented whenever a spawned process is closed
 * or an indirect list is modified.  This forces any (stack of) expect or
 * interact commands to reexamine the state of the world and adjust
 * accordingly.
 */
int exp_configure_count = 0;

/* this message is required because fopen sometimes fails to set errno */
/* Apparently, it "does the user a favor" and doesn't even call open */
/* if the file name is bizarre enough.  This means we can't handle fopen */
/* with the obvious trivial logic. */
static char *open_failed = "could not open - odd file name?";

#ifdef HAVE_PTYTRAP
/* slaveNames provides a mapping from the pty slave names to our */
/* spawn id entry.  This is needed only on HPs for stty, sigh. */
static Tcl_HashTable slaveNames;
#endif /* HAVE_PTYTRAP */

#ifdef FULLTRAPS
static void
init_traps(traps)
RETSIGTYPE (*traps[])();
{
	int i;

	for (i=1;i<NSIG;i++) {
		traps[i] = SIG_ERR;
	}
}
#endif

/* Do not terminate format strings with \n!!! */
/*VARARGS*/
void
exp_error TCL_VARARGS_DEF(Tcl_Interp *,arg1)
/*exp_error(va_alist)*/
/*va_dcl*/
{
	Tcl_Interp *interp;
	char *fmt;
	va_list args;

	interp = TCL_VARARGS_START(Tcl_Interp *,arg1,args);
	/*va_start(args);*/
	/*interp = va_arg(args,Tcl_Interp *);*/
	fmt = va_arg(args,char *);
	vsprintf(interp->result,fmt,args);
	va_end(args);
}

/* returns handle if fd is usable, 0 if not */
struct exp_f *
exp_fd2f(interp,fd,opened,adjust,msg)
Tcl_Interp *interp;
int fd;
int opened;		/* check not closed */
int adjust;		/* adjust buffer sizes */
char *msg;
{
	if (fd >= 0 && fd <= exp_fd_max && (exp_fs[fd].valid)) {
		struct exp_f *f = exp_fs + fd;

		/* following is a little tricky, do not be tempted do the */
		/* 'usual' boolean simplification */
		if ((!opened) || !f->user_closed) {
			if (adjust) exp_adjust(f);
			return f;
		}
	}

	exp_error(interp,"%s: invalid spawn id (%d)",msg,fd);
	return(0);
}

#if 0
/* following routine is not current used, but might be later */
/* returns fd or -1 if no such entry */
static int
pid_to_fd(pid)
int pid;
{
	int fd;

	for (fd=0;fd<=exp_fd_max;fd++) {
		if (exp_fs[fd].pid == pid) return(fd);
	}
	return 0;
}
#endif

/* Tcl needs commands in writable space */
static char close_cmd[] = "close";

/* zero out the wait status field */
static void
exp_wait_zero(status)
WAIT_STATUS_TYPE *status;
{
	int i;

	for (i=0;i<sizeof(WAIT_STATUS_TYPE);i++) {
		((char *)status)[i] = 0;
	}
}

/* prevent an fd from being allocated */
void
exp_busy(fd)
int fd;
{
	int x = open("/dev/null",0);
	if (x != fd) {
		fcntl(x,F_DUPFD,fd);
		close(x);
	}
	exp_close_on_exec(fd);
}

/* called just before an exp_f entry is about to be invalidated */
void
exp_f_prep_for_invalidation(interp,f)
Tcl_Interp *interp;
struct exp_f *f;
{
	int fd = f - exp_fs;

	exp_ecmd_remove_fd_direct_and_indirect(interp,fd);

	exp_configure_count++;

	if (f->buffer) {
		ckfree(f->buffer);
		f->buffer = 0;
		f->msize = 0;
		f->size = 0;
		f->printed = 0;
		f->echoed = 0;
		if (f->fg_armed) {
			exp_event_disarm(f-exp_fs);
			f->fg_armed = FALSE;
		}
		ckfree(f->lower);
	}
	f->fg_armed = FALSE;
}

/*ARGSUSED*/
void
exp_trap_on(master)
int master;
{
#ifdef HAVE_PTYTRAP
	if (master == -1) return;
	exp_slave_control(master,1);
#endif /* HAVE_PTYTRAP */
}

int
exp_trap_off(name)
char *name;
{
#ifdef HAVE_PTYTRAP
	int master;
	struct exp_f *f;
	int enable = 0;

	Tcl_HashEntry *entry = Tcl_FindHashEntry(&slaveNames,name);
	if (!entry) {
		debuglog("exp_trap_off: no entry found for %s\n",name);
		return -1;
	}

	f = (struct exp_f *)Tcl_GetHashValue(entry);
	master = f - exp_fs;

	exp_slave_control(master,0);

	return master;
#else
	return name[0];	/* pacify lint, use arg and return something */
#endif
}

/*ARGSUSED*/
void
sys_close(fd,f)
int fd;
struct exp_f *f;
{
	/* Ignore close errors.  Some systems are really odd and */
	/* return errors for no evident reason.  Anyway, receiving */
	/* an error upon pty-close doesn't mean anything anyway as */
	/* far as I know. */
	close(fd);
	f->sys_closed = TRUE;

#ifdef HAVE_PTYTRAP
	if (f->slave_name) {
		Tcl_HashEntry *entry;

		entry = Tcl_FindHashEntry(&slaveNames,f->slave_name);
		Tcl_DeleteHashEntry(entry);

		ckfree(f->slave_name);
		f->slave_name = 0;
	}
#endif
}

/* given a Tcl file identifier, close it */
static void
close_tcl_file(interp,file_id)
Tcl_Interp *interp;
char *file_id;
{
    Tcl_VarEval(interp,"close ",file_id,(char *)0);

#if 0  /* old Tcl 7.6 code */
	char *argv[3];
	Tcl_CmdInfo info;

	argv[0] = close_cmd;
	argv[1] = file_id;
	argv[2] = 0;

	Tcl_ResetResult(interp);
	Tcl_GetCommandInfo(interp,"close",&info);
	if (0 == Tcl_GetCommandInfo(interp,"close",&info)) {
		info.clientData = 0;
	}
	(void) Tcl_CloseCmd(info.clientData,interp,2,argv);
#endif
}			


/* close all connections
The kernel would actually do this by default, however Tcl is going to
come along later and try to reap its exec'd processes.  If we have
inherited any via spawn -open, Tcl can hang if we don't close the
connections first.
*/

void
exp_close_all(interp)
Tcl_Interp *interp;
{
	int fd;

	for (fd=0;fd<=exp_fd_max;fd++) {
		if (exp_fs[fd].valid) {
			exp_close(interp,fd);
		}
	}
}

int
exp_close(interp,fd)
Tcl_Interp *interp;
int fd;
{
	struct exp_f *f = exp_fd2f(interp,fd,1,0,"close");
	if (!f) return(TCL_ERROR);

	f->user_closed = TRUE;

	if (f->slave_fd != EXP_NOFD) close(f->slave_fd);
#if 0
	if (f->tcl_handle) {
		ckfree(f->tcl_handle);
		if ((f - exp_fs) != f->tcl_output) close(f->tcl_output);
	}
#endif
	sys_close(fd,f);

	if (f->tcl_handle) {
		if ((f - exp_fs) != f->tcl_output) close(f->tcl_output);

		if (!f->leaveopen) {
			/*
			 * Ignore errors from close; they report things like
			 * broken pipeline, etc, which don't affect our
			 * subsequent handling.
			 */

			close_tcl_file(interp,f->tcl_handle);

			ckfree(f->tcl_handle);
			f->tcl_handle = 0;
		}
	}

	exp_f_prep_for_invalidation(interp,f);

	if (f->user_waited) {
		f->valid = FALSE;
	} else {
		exp_busy(fd);
		f->sys_closed = FALSE;
	}

	return(TCL_OK);
}

static struct exp_f *
fd_new(fd,pid)
int fd;
int pid;
{
	int i, low;
	struct exp_f *newfs;	/* temporary, so we don't lose old exp_fs */

	/* resize table if nec */
	if (fd > exp_fd_max) {
		if (!exp_fs) {	/* no fd's yet allocated */
			newfs = (struct exp_f *)ckalloc(sizeof(struct exp_f)*(fd+1));
			low = 0;
		} else {		/* enlarge fd table */
			newfs = (struct exp_f *)ckrealloc((char *)exp_fs,sizeof(struct exp_f)*(fd+1));
			low = exp_fd_max+1;
		}
		exp_fs = newfs;
		exp_fd_max = fd;
		for (i = low; i <= exp_fd_max; i++) { /* init new fd entries */
			exp_fs[i].valid = FALSE;
			exp_fs[i].fd_ptr = (int *)ckalloc(sizeof(int));
			*exp_fs[i].fd_ptr = i;

/*			exp_fs[i].ptr = (struct exp_f **)ckalloc(sizeof(struct exp_fs *));*/

		}

#if 0
		for (i = 0; i <= exp_fd_max; i++) { /* update all indirect ptrs */
			*exp_fs[i].ptr = exp_fs + i;
		}
#endif
	}

	/* this could happen if user does "spawn -open stdin" I suppose */
	if (exp_fs[fd].valid) return exp_fs+fd;

	/* close down old table entry if nec */
	exp_fs[fd].pid = pid;
	exp_fs[fd].size = 0;
	exp_fs[fd].msize = 0;
	exp_fs[fd].buffer = 0;
	exp_fs[fd].printed = 0;
	exp_fs[fd].echoed = 0;
	exp_fs[fd].rm_nulls = exp_default_rm_nulls;
	exp_fs[fd].parity = exp_default_parity;
	exp_fs[fd].key = expect_key++;
	exp_fs[fd].force_read = FALSE;
	exp_fs[fd].fg_armed = FALSE;
#if TCL_MAJOR_VERSION < 8
	/* Master must be inited each time because Tcl could have alloc'd */
	/* this fd and shut it down (deallocating the FileHandle) behind */
	/* our backs */
        exp_fs[fd].Master = Tcl_GetFile((ClientData)fd,TCL_UNIX_FD);
        exp_fs[fd].MasterOutput = 0;
        exp_fs[fd].Slave = 0;
#endif /* TCL_MAJOR_VERSION < 8 */
#ifdef __CYGWIN32__
       exp_fs[fd].channel = NULL;
       exp_fs[fd].fileproc = NULL;
#endif
	exp_fs[fd].tcl_handle = 0;
	exp_fs[fd].slave_fd = EXP_NOFD;
#ifdef HAVE_PTYTRAP
	exp_fs[fd].slave_name = 0;
#endif /* HAVE_PTYTRAP */
	exp_fs[fd].umsize = exp_default_match_max;
	exp_fs[fd].valid = TRUE;
	exp_fs[fd].user_closed = FALSE;
	exp_fs[fd].sys_closed = FALSE;
	exp_fs[fd].user_waited = FALSE;
	exp_fs[fd].sys_waited = FALSE;
	exp_fs[fd].bg_interp = 0;
	exp_fs[fd].bg_status = unarmed;
	exp_fs[fd].bg_ecount = 0;

	return exp_fs+fd;
}

#if 0
void
exp_global_init(eg,duration,location)
struct expect_global *eg;
int duration;
int location;
{
	eg->ecases = 0;
	eg->ecount = 0;
	eg->i_list = 0;
	eg->duration = duration;
	eg->location = location;
}
#endif

void
exp_init_spawn_id_vars(interp)
Tcl_Interp *interp;
{
	Tcl_SetVar(interp,"user_spawn_id",EXP_SPAWN_ID_USER_LIT,0);
	Tcl_SetVar(interp,"error_spawn_id",EXP_SPAWN_ID_ERROR_LIT,0);

	/* note that the user_spawn_id is NOT /dev/tty which could */
	/* (at least in theory anyway) be later re-opened on a different */
	/* fd, while stdin might have been redirected away from /dev/tty */

	if (exp_dev_tty != -1) {
		char dev_tty_str[10];
		sprintf(dev_tty_str,"%d",exp_dev_tty);
		Tcl_SetVar(interp,"tty_spawn_id",dev_tty_str,0);
	}
}

void
exp_init_spawn_ids()
{
	/* note whether 0,1,2 are connected to a terminal so that if we */
	/* disconnect, we can shut these down.  We would really like to */
	/* test if 0,1,2 are our controlling tty, but I don't know any */
	/* way to do that portably.  Anyway, the likelihood of anyone */
	/* disconnecting after redirecting to a non-controlling tty is */
	/* virtually zero. */

	fd_new(0,isatty(0)?exp_getpid:EXP_NOPID);
	fd_new(1,isatty(1)?exp_getpid:EXP_NOPID);
	fd_new(2,isatty(2)?exp_getpid:EXP_NOPID);

	if (exp_dev_tty != -1) {
		fd_new(exp_dev_tty,exp_getpid);
	}

	/* really should be in interpreter() but silly to do on every call */
	exp_adjust(&exp_fs[0]);
}

void
exp_close_on_exec(fd)
int fd;
{
	(void) fcntl(fd,F_SETFD,1);
}

#define STTY_INIT	"stty_init"

#if 0
static void
show_pgrp(fd,string)
int fd;
char *string;
{
	int pgrp;

	fprintf(stderr,"getting pgrp for %s\n",string);
	if (-1 == ioctl(fd,TIOCGETPGRP,&pgrp)) perror("TIOCGETPGRP");
	else fprintf(stderr,"%s pgrp = %d\n",string,pgrp);
	if (-1 == ioctl(fd,TIOCGPGRP,&pgrp)) perror("TIOCGPGRP");
	else fprintf(stderr,"%s pgrp = %d\n",string,pgrp);
	if (-1 == tcgetpgrp(fd,pgrp)) perror("tcgetpgrp");
	else fprintf(stderr,"%s pgrp = %d\n",string,pgrp);
}

static void
set_pgrp(fd)
int fd;
{
	int pgrp = getpgrp(0);
	if (-1 == ioctl(fd,TIOCSETPGRP,&pgrp)) perror("TIOCSETPGRP");
	if (-1 == ioctl(fd,TIOCSPGRP,&pgrp)) perror("TIOCSPGRP");
	if (-1 == tcsetpgrp(fd,pgrp)) perror("tcsetpgrp");
}
#endif

/*ARGSUSED*/
static void
set_slave_name(f,name)
struct exp_f *f;
char *name;
{
#ifdef HAVE_PTYTRAP
	int newptr;
	Tcl_HashEntry *entry;

	/* save slave name */
	f->slave_name = ckalloc(strlen(exp_pty_slave_name)+1);
	strcpy(f->slave_name,exp_pty_slave_name);

	entry = Tcl_CreateHashEntry(&slaveNames,exp_pty_slave_name,&newptr);
	Tcl_SetHashValue(entry,(ClientData)f);
#endif /* HAVE_PTYTRAP */
}

#ifdef __CYGWIN32__
/* Sometimes, the win32 version of expect passes a windows handle to
   dup(), which normally only takes file descriptors.  We check for
   that with this wrapper.  DJ */
#include <windows.h>
static int
cygwin_pipe_dup (int oldfd)
{
  int rv = dup(oldfd);
  if (rv != -1) /* cool */
    return rv;
  /* Oops, check for a handle */
  if (GetFileType((HANDLE)oldfd) == FILE_TYPE_PIPE)
    {
      if (DuplicateHandle(GetCurrentProcess(),
			  (HANDLE)oldfd,
			  GetCurrentProcess(),
			  (HANDLE *)&rv,
			  0, 0,
			  DUPLICATE_SAME_ACCESS))
	{
	  int fd = cygwin32_attach_handle_to_fd ("/dev/piped",
						 -1, rv,
						 1, O_RDWR);
	  if (fd >= 0)
	    return fd;
	}
    }
  return -1;
}
#endif

/* arguments are passed verbatim to execvp() */
/*ARGSUSED*/
static int
Exp_SpawnCmd(clientData,interp,argc,argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int slave;
	int pid;
	char **a;
	/* tell Saber to ignore non-use of ttyfd */
	/*SUPPRESS 591*/
	int errorfd;	/* place to stash fileno(stderr) in child */
			/* while we're setting up new stderr */
	int ttyfd;
	int master;
	int write_master;	/* write fd of Tcl-opened files */
	int ttyinit = TRUE;
	int ttycopy = TRUE;
	int echo = TRUE;
	int console = FALSE;
	int pty_only = FALSE;

#ifdef FULLTRAPS
				/* Allow user to reset signals in child */
				/* The following array contains indicates */
				/* whether sig should be DFL or IGN */
				/* ERR is used to indicate no initialization */
	RETSIGTYPE (*traps[NSIG])();
#endif
	int ignore[NSIG];	/* if true, signal in child is ignored */
				/* if false, signal gets default behavior */
	int i;			/* trusty overused temporary */

	char *argv0 = argv[0];
	char *openarg = 0;
	int leaveopen = FALSE;
	FILE *readfilePtr;
	FILE *writefilePtr;
	int rc, wc;
	char *stty_init;
	int slave_write_ioctls = 1;
		/* by default, slave will be write-ioctled this many times */
	int slave_opens = 3;
		/* by default, slave will be opened this many times */
		/* first comes from initial allocation */
		/* second comes from stty */
		/* third is our own signal that stty is done */

	int sync_fds[2];
	int sync2_fds[2];
	int status_pipe[2];
	int child_errno;
	char sync_byte;

	char buf[4];		/* enough space for a string literal */
				/* representing a file descriptor */
	Tcl_DString dstring;
	Tcl_DStringInit(&dstring);

#ifdef FULLTRAPS
	init_traps(&traps);
#endif
	/* don't ignore any signals in child by default */
	for (i=1;i<NSIG;i++) {
		ignore[i] = FALSE;
	}

	argc--; argv++;

	for (;argc>0;argc--,argv++) {
		if (streq(*argv,"-nottyinit")) {
			ttyinit = FALSE;
			slave_write_ioctls--;
			slave_opens--;
		} else if (streq(*argv,"-nottycopy")) {
			ttycopy = FALSE;
		} else if (streq(*argv,"-noecho")) {
			echo = FALSE;
		} else if (streq(*argv,"-console")) {
			console = TRUE;
		} else if (streq(*argv,"-pty")) {
			pty_only = TRUE;
		} else if (streq(*argv,"-open")) {
			if (argc < 2) {
				exp_error(interp,"usage: -open file-identifier");
				return TCL_ERROR;
			}
			openarg = argv[1];
			argc--; argv++;
		} else if (streq(*argv,"-leaveopen")) {
			if (argc < 2) {
				exp_error(interp,"usage: -open file-identifier");
				return TCL_ERROR;
			}
			openarg = argv[1];
			leaveopen = TRUE;
			argc--; argv++;
		} else if (streq(*argv,"-ignore")) {
			int sig;

			if (argc < 2) {
				exp_error(interp,"usage: -ignore signal");
				return TCL_ERROR;
			}
			sig = exp_string_to_signal(interp,argv[1]);
			if (sig == -1) {
				exp_error(interp,"usage: -ignore %s: unknown signal name",argv[1]);
				return TCL_ERROR;
			}
			ignore[sig] = TRUE;
			argc--; argv++;
#ifdef FULLTRAPS
		} else if (streq(*argv,"-trap")) {
			/* argv[1] is action */
			/* argv[2] is list of signals */

			RETSIGTYPE (*sig_handler)();
			int n;		/* number of signals in list */
			char **list;	/* list of signals */

			if (argc < 3) {
				exp_error(interp,"usage: -trap siglist SIG_DFL or SIG_IGN");
				return TCL_ERROR;
			}

			if (0 == strcmp(argv[2],"SIG_DFL")) {
				sig_handler = SIG_DFL;
			} else if (0 == strcmp(argv[2],"SIG_IGN")) {
				sig_handler = SIG_IGN;
			} else {
				exp_error(interp,"usage: -trap siglist SIG_DFL or SIG_IGN");
				return TCL_ERROR;
			}

			if (TCL_OK != Tcl_SplitList(interp,argv[1],&n,&list)) {
				errorlog("%s\r\n",interp->result);
				exp_error(interp,"usage: -trap {siglist} ...");
				return TCL_ERROR;
			}
			for (i=0;i<n;i++) {
				int sig = exp_string_to_signal(interp,list[i]);
				if (sig == -1) {
					ckfree((char *)&list);
					return TCL_ERROR;
				}
				traps[sig] = sig_handler;
			}
			ckfree((char *)&list);

			argc--; argv++;
			argc--; argv++;
#endif /*FULLTRAPS*/
		} else break;
	}

	if (openarg && (argc != 0)) {
		exp_error(interp,"usage: -[leave]open [fileXX]");
		return TCL_ERROR;
	}

	if (!pty_only && !openarg && (argc == 0)) {
		exp_error(interp,"usage: spawn [spawn-args] program [program-args]");
		return(TCL_ERROR);
	}

	stty_init = exp_get_var(interp,STTY_INIT);
	if (stty_init) {
		slave_write_ioctls++;
		slave_opens++;
	}

/* any extraneous ioctl's that occur in slave must be accounted for
when trapping, see below in child half of fork */
#if defined(TIOCSCTTY) && !defined(CIBAUD) && !defined(sun) && !defined(hp9000s300)
	slave_write_ioctls++;
	slave_opens++;
#endif

	exp_pty_slave_name = 0;

	Tcl_ReapDetachedProcs();

	if (!openarg) {
		if (echo) {
			exp_log(0,"%s ",argv0);
			for (a = argv;*a;a++) {
				exp_log(0,"%s ",*a);
			}
			exp_nflog("\r\n",0);
		}

		if (0 > (master = getptymaster())) {
			/*
			 * failed to allocate pty, try and figure out why
			 * so we can suggest to user what to do about it.
			 */

			int count;
			int testfd;

			if (exp_pty_error) {
				exp_error(interp,"%s",exp_pty_error);
				return TCL_ERROR;
			}

			count = 0;
			for (i=3;i<=exp_fd_max;i++) {
				count += exp_fs[i].valid;
			}
			if (count > 10) {
				exp_error(interp,"The system only has a finite number of ptys and you have many of them in use.  The usual reason for this is that you forgot (or didn't know) to call \"wait\" after closing each of them.");
				return TCL_ERROR;
			}

			testfd = open("/",0);
			close(testfd);

			if (testfd != -1) {
				exp_error(interp,"The system has no more ptys.  Ask your system administrator to create more.");
			} else {
				exp_error(interp,"- You have too many files are open.  Close some files or increase your per-process descriptor limit.");
			}
			return(TCL_ERROR);
		}
#ifdef PTYTRAP_DIES
		if (!pty_only) exp_slave_control(master,1);
#endif /* PTYTRAP_DIES */

#define SPAWN_OUT "spawn_out"
		Tcl_SetVar2(interp,SPAWN_OUT,"slave,name",exp_pty_slave_name,0);
	} else {
		Tcl_Channel chan;
		int mode;
#if TCL_MAJOR_VERSION < 8
		Tcl_File tclReadFile, tclWriteFile;
#endif /* TCL_MAJOR_VERSION < 8 */
		/* CYGNUS LOCAL 64bit/law */
		/* These must be both wide enough and aligned enough for
		   the TCL code to store a pointer into them!  */
		void *rfd, *wfd;
		/* END CYGNUS LOCAL */

		if (echo) exp_log(0,"%s [open ...]\r\n",argv0);

#if TCL7_4
		rc = Tcl_GetOpenFile(interp,openarg,0,1,&readfilePtr);
		wc = Tcl_GetOpenFile(interp,openarg,1,1,&writefilePtr);

		/* fail only if both descriptors are bad */
		if (rc == TCL_ERROR && wc == TCL_ERROR) {
			return TCL_ERROR;		
		}

		master = fileno((rc == TCL_OK)?readfilePtr:writefilePtr);

		/* make a new copy of file descriptor */
		if (-1 == (write_master = master = dup(master))) {
			exp_error(interp,"fdopen: %s",Tcl_PosixError(interp));
			return TCL_ERROR;
		}

		/* if writefilePtr is different, dup that too */
		if ((rc == TCL_OK) && (wc == TCL_OK) && (fileno(writefilePtr) != fileno(readfilePtr))) {
			if (-1 == (write_master = dup(fileno(writefilePtr)))) {
				exp_error(interp,"fdopen: %s",Tcl_PosixError(interp));
				return TCL_ERROR;
			}
			exp_close_on_exec(write_master);
		}

#endif
		if (!(chan = Tcl_GetChannel(interp,openarg,&mode))) {
			return TCL_ERROR;
		}
		if (!mode) {
			exp_error(interp,"channel is neither readable nor writable");
			return TCL_ERROR;
		}
		if (mode & TCL_READABLE) {
#if TCL_MAJOR_VERSION < 8
			tclReadFile = Tcl_GetChannelFile(chan, TCL_READABLE);
			rfd = (int)Tcl_GetFileInfo(tclReadFile, (int *)0);
#else
			if (TCL_ERROR == Tcl_GetChannelHandle(chan, TCL_READABLE, (ClientData) &rfd)) {
				return TCL_ERROR;
			}
#endif /* TCL_MAJOR_VERSION < 8 */
		}
		if (mode & TCL_WRITABLE) {
#if TCL_MAJOR_VERSION < 8
			tclWriteFile = Tcl_GetChannelFile(chan, TCL_WRITABLE);
			wfd = (int)Tcl_GetFileInfo(tclWriteFile, (int *)0);
#else
			if (TCL_ERROR == Tcl_GetChannelHandle(chan, TCL_WRITABLE, (ClientData) &wfd)) {
				return TCL_ERROR;
			}
#endif /* TCL_MAJOR_VERSION < 8 */
		}

		master = ((mode & TCL_READABLE)?rfd:wfd);

		/* make a new copy of file descriptor */
#ifdef __CYGWIN32__
		if (-1 == (write_master = master = cygwin_pipe_dup(master))) {
#else
		if (-1 == (write_master = master = dup(master))) {
#endif
			exp_error(interp,"fdopen: %s",Tcl_PosixError(interp));
			return TCL_ERROR;
		}

		/* if writefilePtr is different, dup that too */
		if ((mode & TCL_READABLE) && (mode & TCL_WRITABLE) && (wfd != rfd)) {
			if (-1 == (write_master = dup(wfd))) {
				exp_error(interp,"fdopen: %s",Tcl_PosixError(interp));
				return TCL_ERROR;
			}
			exp_close_on_exec(write_master);
		}

		/*
		 * It would be convenient now to tell Tcl to close its
		 * file descriptor.  Alas, if involved in a pipeline, Tcl
		 * will be unable to complete a wait on the process.
		 * So simply remember that we meant to close it.  We will
		 * do so later in our own close routine.
		 */
	}

	/* much easier to set this, than remember all masters */
	exp_close_on_exec(master);

	if (openarg || pty_only) {
		struct exp_f *f;

		f = fd_new(master,EXP_NOPID);

		if (openarg) {
			/* save file# handle */
			f->tcl_handle = ckalloc(strlen(openarg)+1);
			strcpy(f->tcl_handle,openarg);

			f->tcl_output = write_master;
#if 0
			/* save fd handle for output */
			if (wc == TCL_OK) {
/*				f->tcl_output = fileno(writefilePtr);*/
				f->tcl_output = write_master;
			} else {
				/* if we actually try to write to it at some */
				/* time in the future, then this will cause */
				/* an error */
				f->tcl_output = master;
			}
#endif

			f->leaveopen = leaveopen;
		}

		if (exp_pty_slave_name) set_slave_name(f,exp_pty_slave_name);

		/* make it appear as if process has been waited for */
		f->sys_waited = TRUE;
		exp_wait_zero(&f->wait);

		/* tell user id of new process */
		sprintf(buf,"%d",master);
		Tcl_SetVar(interp,SPAWN_ID_VARNAME,buf,0);

		if (!openarg) {
			char value[20];
			int dummyfd1, dummyfd2;

			/*
			 * open the slave side in the same process to support
			 * the -pty flag.
			 */

			/* Start by working around a bug in Tcl's exec.
			   It closes all the file descriptors from 3 to it's
			   own fd_max which inappropriately closes our slave
			   fd.  To avoid this, open several dummy fds.  Then
			   exec's fds will fall below ours.
			   Note that if you do something like pre-allocating
			   a bunch before using them or generating a pipeline,
			   then this code won't help.
			   Instead you'll need to add the right number of
			   explicit Tcl open's of /dev/null.
			   The right solution is fix Tcl's exec so it is not
			   so cavalier.
			 */

			dummyfd1 = open("/dev/null",0);
			dummyfd2 = open("/dev/null",0);

			if (0 > (f->slave_fd = getptyslave(ttycopy,ttyinit,
					stty_init))) {
				exp_error(interp,"open(slave pty): %s\r\n",Tcl_PosixError(interp));
				return TCL_ERROR;
			}

			close(dummyfd1);
			close(dummyfd2);

			exp_slave_control(master,1);

			sprintf(value,"%d",f->slave_fd);
			Tcl_SetVar2(interp,SPAWN_OUT,"slave,fd",value,0);
		}
		sprintf(interp->result,"%d",EXP_NOPID);
		debuglog("spawn: returns {%s}\r\n",interp->result);

		return TCL_OK;
	}

	if (NULL == (argv[0] = Tcl_TildeSubst(interp,argv[0],&dstring))) {
		goto parent_error;
	}

	if (-1 == pipe(sync_fds)) {
		exp_error(interp,"too many programs spawned?  could not create pipe: %s",Tcl_PosixError(interp));
		goto parent_error;
	}

	if (-1 == pipe(sync2_fds)) {
		close(sync_fds[0]);
		close(sync_fds[1]);
		exp_error(interp,"too many programs spawned?  could not create pipe: %s",Tcl_PosixError(interp));
		goto parent_error;
	}

	if (-1 == pipe(status_pipe)) {
		close(sync_fds[0]);
		close(sync_fds[1]);
		close(sync2_fds[0]);
		close(sync2_fds[1]);
	}

	if ((pid = fork()) == -1) {
		exp_error(interp,"fork: %s",Tcl_PosixError(interp));
		goto parent_error;
	}

	if (pid) { /* parent */
		struct exp_f *f;

		close(sync_fds[1]);
		close(sync2_fds[0]);
		close(status_pipe[1]);

		f = fd_new(master,pid);

		if (exp_pty_slave_name) set_slave_name(f,exp_pty_slave_name);

#ifdef CRAY
		setptypid(pid);
#endif


#if PTYTRAP_DIES
#ifdef HAVE_PTYTRAP

		while (slave_opens) {
			int cc;
			cc = exp_wait_for_slave_open(master);
#if defined(TIOCSCTTY) && !defined(CIBAUD) && !defined(sun) && !defined(hp9000s300)
			if (cc == TIOCSCTTY) slave_opens = 0;
#endif
			if (cc == TIOCOPEN) slave_opens--;
			if (cc == -1) {
				exp_error(interp,"failed to trap slave pty");
				goto parent_error;
			}
		}

#if 0
		/* trap initial ioctls in a feeble attempt to not block */
		/* the initially.  If the process itself ioctls */
		/* /dev/tty, such blocks will be trapped later */
		/* during normal event processing */

		/* initial slave ioctl */
		while (slave_write_ioctls) {
			int cc;

			cc = exp_wait_for_slave_open(master);
#if defined(TIOCSCTTY) && !defined(CIBAUD) && !defined(sun) && !defined(hp9000s300)
			if (cc == TIOCSCTTY) slave_write_ioctls = 0;
#endif
			if (cc & IOC_IN) slave_write_ioctls--;
			else if (cc == -1) {
				exp_error(interp,"failed to trap slave pty");
				goto parent_error;
			}
		}
#endif /*0*/

#endif /* HAVE_PTYTRAP */
#endif /* PTYTRAP_DIES */

		/*
		 * wait for slave to initialize pty before allowing
		 * user to send to it
		 */ 

		debuglog("parent: waiting for sync byte\r\n");
		while (((rc = read(sync_fds[0],&sync_byte,1)) < 0) && (errno == EINTR)) {
			/* empty */;
		}
		if (rc == -1) {
			errorlog("parent: sync byte read: %s\r\n",Tcl_ErrnoMsg(errno));
			exit(-1);
		}

		/* turn on detection of eof */
		exp_slave_control(master,1);

		/*
		 * tell slave to go on now now that we have initialized pty
		 */

		debuglog("parent: telling child to go ahead\r\n");
		wc = write(sync2_fds[1]," ",1);
		if (wc == -1) {
			errorlog("parent: sync byte write: %s\r\n",Tcl_ErrnoMsg(errno));
			exit(-1);
		}

		debuglog("parent: now unsynchronized from child\r\n");
		close(sync_fds[0]);
		close(sync2_fds[1]);

		/* see if child's exec worked */
	retry:
		switch (read(status_pipe[0],&child_errno,sizeof child_errno)) {
		case -1:
			if (errno == EINTR) goto retry;
			/* well it's not really the child's errno */
			/* but it can be treated that way */
			child_errno = errno;
			break;
		case 0:
			/* child's exec succeeded */
			child_errno = 0;
			break;
		default:
			/* child's exec failed; err contains exec's errno  */
			waitpid(pid, NULL, 0);
			/* in order to get Tcl to set errorcode, we must */
			/* hand set errno */
			errno = child_errno;
			exp_error(interp, "couldn't execute \"%s\": %s",
				argv[0],Tcl_PosixError(interp));
			goto parent_error;
		}
		close(status_pipe[0]);


		/* tell user id of new process */
		sprintf(buf,"%d",master);
		Tcl_SetVar(interp,SPAWN_ID_VARNAME,buf,0);

		sprintf(interp->result,"%d",pid);
		debuglog("spawn: returns {%s}\r\n",interp->result);

		Tcl_DStringFree(&dstring);
		return(TCL_OK);
parent_error:
		Tcl_DStringFree(&dstring);
		return TCL_ERROR;
	}

	/* child process - do not return from here!  all errors must exit() */

	close(sync_fds[0]);
	close(sync2_fds[1]);
	close(status_pipe[0]);
	exp_close_on_exec(status_pipe[1]);

	if (exp_dev_tty != -1) {
		close(exp_dev_tty);
		exp_dev_tty = -1;
	}

#ifdef CRAY
	(void) close(master);
#endif

/* ultrix (at least 4.1-2) fails to obtain controlling tty if setsid */
/* is called.  setpgrp works though.  */
#if defined(POSIX) && !defined(ultrix)
#define DO_SETSID
#endif
#ifdef __convex__
#define DO_SETSID
#endif

#ifdef DO_SETSID
	setsid();
#else
#ifdef SYSV3
#ifndef CRAY
	setpgrp();
#endif /* CRAY */
#else /* !SYSV3 */
#ifdef MIPS_BSD
	/* required on BSD side of MIPS OS <jmsellen@watdragon.waterloo.edu> */
#	include <sysv/sys.s>
	syscall(SYS_setpgrp);
#endif
	setpgrp(0,0);
/*	setpgrp(0,getpid());*/	/* make a new pgrp leader */

/* Pyramid lacks this defn */
#ifdef TIOCNOTTY
	ttyfd = open("/dev/tty", O_RDWR);
	if (ttyfd >= 0) {
		(void) ioctl(ttyfd, TIOCNOTTY, (char *)0);
		(void) close(ttyfd);
	}
#endif /* TIOCNOTTY */

#endif /* SYSV3 */
#endif /* DO_SETSID */

	/* save stderr elsewhere to avoid BSD4.4 bogosity that warns */
	/* if stty finds dev(stderr) != dev(stdout) */

	/* save error fd while we're setting up new one */
	errorfd = fcntl(2,F_DUPFD,3);
	/* and here is the macro to restore it */
#define restore_error_fd {close(2);fcntl(errorfd,F_DUPFD,2);}

	close(0);
	close(1);
	close(2);

	/* since we closed fd 0, open of pty slave must return fd 0 */

	/* since getptyslave may have to run stty, (some of which work on fd */
	/* 0 and some of which work on 1) do the dup's inside getptyslave. */

	if (0 > (slave = getptyslave(ttycopy,ttyinit,stty_init))) {
		restore_error_fd
		errorlog("open(slave pty): %s\r\n",Tcl_ErrnoMsg(errno));
		exit(-1);
	}
	/* sanity check */
	if (slave != 0) {
		restore_error_fd
		errorlog("getptyslave: slave = %d but expected 0\n",slave);
		exit(-1);
	}

/* The test for hpux may have to be more specific.  In particular, the */
/* code should be skipped on the hp9000s300 and hp9000s720 (but there */
/* is no documented define for the 720!) */

/*#if defined(TIOCSCTTY) && !defined(CIBAUD) && !defined(sun) && !defined(hpux)*/
#if defined(TIOCSCTTY) && !defined(sun) && !defined(hpux)
	/* 4.3+BSD way to acquire controlling terminal */
	/* according to Stevens - Adv. Prog..., p 642 */
	/* Oops, it appears that the CIBAUD is on Linux also */
	/* so let's try without... */
#ifdef __QNX__
	if (tcsetct(0, getpid()) == -1) {
#else
	if (ioctl(0,TIOCSCTTY,(char *)0) < 0) {
#endif
		restore_error_fd
		errorlog("failed to get controlling terminal using TIOCSCTTY");
		exit(-1);
	}
#endif

#ifdef CRAY
 	(void) setsid();
 	(void) ioctl(0,TCSETCTTY,0);
 	(void) close(0);
 	if (open("/dev/tty", O_RDWR) < 0) {
		restore_error_fd
 		errorlog("open(/dev/tty): %s\r\n",Tcl_ErrnoMsg(errno));
 		exit(-1);
 	}
 	(void) close(1);
 	(void) close(2);
 	(void) dup(0);
 	(void) dup(0);
	setptyutmp();	/* create a utmp entry */

	/* _CRAY2 code from Hal Peterson <hrp@cray.com>, Cray Research, Inc. */
#ifdef _CRAY2
	/*
	 * Interpose a process between expect and the spawned child to
	 * keep the slave side of the pty open to allow time for expect
	 * to read the last output.  This is a workaround for an apparent
	 * bug in the Unicos pty driver on Cray-2's under Unicos 6.0 (at
	 * least).
	 */
	if ((pid = fork()) == -1) {
		restore_error_fd
		errorlog("second fork: %s\r\n",Tcl_ErrnoMsg(errno));
		exit(-1);
	}

	if (pid) {
 		/* Intermediate process. */
		int status;
		int timeout;
		char *t;

		/* How long should we wait? */
		if (t = exp_get_var(interp,"pty_timeout"))
			timeout = atoi(t);
		else if (t = exp_get_var(interp,"timeout"))
			timeout = atoi(t)/2;
		else
			timeout = 5;

		/* Let the spawned process run to completion. */
 		while (wait(&status) < 0 && errno == EINTR)
			/* empty body */;

		/* Wait for the pty to clear. */
		sleep(timeout);

		/* Duplicate the spawned process's status. */
		if (WIFSIGNALED(status))
			kill(getpid(), WTERMSIG(status));

		/* The kill may not have worked, but this will. */
 		exit(WEXITSTATUS(status));
	}
#endif /* _CRAY2 */
#endif /* CRAY */

	if (console) exp_console_set();

#ifdef FULLTRAPS
	for (i=1;i<NSIG;i++) {
		if (traps[i] != SIG_ERR) {
			signal(i,traps[i]);
		}
	}
#endif /* FULLTRAPS */

	for (i=1;i<NSIG;i++) {
		signal(i,ignore[i]?SIG_IGN:SIG_DFL);
	}

#if 0
	/* avoid fflush of cmdfile since this screws up the parents seek ptr */
	/* There is no portable way to fclose a shared read-stream!!!! */
	if (exp_cmdfile && (exp_cmdfile != stdin))
		(void) close(fileno(exp_cmdfile));
	if (logfile) (void) fclose(logfile);
	if (debugfile) (void) fclose(debugfile);
#endif
	/* (possibly multiple) masters are closed automatically due to */
	/* earlier fcntl(,,CLOSE_ON_EXEC); */

	/* tell parent that we are done setting up pty */
	/* The actual char sent back is irrelevant. */

	/* debuglog("child: telling parent that pty is initialized\r\n");*/
	wc = write(sync_fds[1]," ",1);
	if (wc == -1) {
		restore_error_fd
		errorlog("child: sync byte write: %s\r\n",Tcl_ErrnoMsg(errno));
		exit(-1);
	}
	close(sync_fds[1]);

	/* wait for master to let us go on */
	/* debuglog("child: waiting for go ahead from parent\r\n"); */

/*	close(master);	/* force master-side close so we can read */

	while (((rc = read(sync2_fds[0],&sync_byte,1)) < 0) && (errno == EINTR)) {
		/* empty */;
	}

	if (rc == -1) {
		restore_error_fd
		errorlog("child: sync byte read: %s\r\n",Tcl_ErrnoMsg(errno));
		exit(-1);
	}
	close(sync2_fds[0]);

	/* debuglog("child: now unsynchronized from parent\r\n"); */

	/* So much for close-on-exec.  Tcl doesn't mark its files that way */
	/* everything has to be closed explicitly. */
	if (exp_close_in_child) (*exp_close_in_child)();

        (void) execvp(argv[0],argv);
#if 0
	/* Unfortunately, by now we've closed fd's to stderr, logfile and
		debugfile.
	   The only reasonable thing to do is to send back the error as
	   part of the program output.  This will be picked up in an
	   expect or interact command.
	*/
	errorlog("%s: %s\r\n",argv[0],Tcl_ErrnoMsg(errno));
#endif
	/* if exec failed, communicate the reason back to the parent */
	write(status_pipe[1], &errno, sizeof errno);
	exit(-1);
	/*NOTREACHED*/
}

/*ARGSUSED*/
static int
Exp_ExpPidCmd(clientData,interp,argc,argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct exp_f *f;
	int m = -1;

	argc--; argv++;

	for (;argc>0;argc--,argv++) {
		if (streq(*argv,"-i")) {
			argc--; argv++;
			if (!*argv) goto usage;
			m = atoi(*argv);
		} else goto usage;
	}

	if (m == -1) {
		if (exp_update_master(interp,&m,0,0) == 0) return TCL_ERROR;
	}

	if (0 == (f = exp_fd2f(interp,m,1,0,"exp_pid"))) return TCL_ERROR;

	sprintf(interp->result,"%d",f->pid);
	return TCL_OK;
 usage:
	exp_error(interp,"usage: -i spawn_id");
	return TCL_ERROR;
}

/*ARGSUSED*/
static int
Exp_GetpidDeprecatedCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	debuglog("getpid is deprecated, use pid\r\n");
	sprintf(interp->result,"%d",getpid());
	return(TCL_OK);
}

/* returns current master (via out-parameter) */
/* returns f or 0, but note that since exp_fd2f calls tcl_error, this */
/* may be immediately followed by a "return(TCL_ERROR)"!!! */
struct exp_f *
exp_update_master(interp,m,opened,adjust)
Tcl_Interp *interp;
int *m;
int opened;
int adjust;
{
	char *s = exp_get_var(interp,SPAWN_ID_VARNAME);
	*m = (s?atoi(s):EXP_SPAWN_ID_USER);
	return(exp_fd2f(interp,*m,opened,adjust,(s?s:EXP_SPAWN_ID_USER_LIT)));
}

/*ARGSUSED*/
static int
Exp_SleepCmd(clientData,interp,argc,argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	argc--; argv++;

	if (argc != 1) {
		exp_error(interp,"must have one arg: seconds");
		return TCL_ERROR;
	}

	return(exp_dsleep(interp,(double)atof(*argv)));
}

/* write exactly this many bytes, i.e. retry partial writes */
/* returns 0 for success, -1 for failure */
static int
exact_write(fd,buffer,rembytes)
int fd;
char *buffer;
int rembytes;
{
	int cc;

	while (rembytes) {
		if (-1 == (cc = write(fd,buffer,rembytes))) return(-1);
		if (0 == cc) {
			/* This shouldn't happen but I'm told that it does */
			/* nonetheless (at least on SunOS 4.1.3).  Since */
			/* this is not a documented return value, the most */
			/* reasonable thing is to complain here and retry */
			/* in the hopes that is some transient condition. */
			sleep(1);
			exp_debuglog("write() failed to write anything but returned - sleeping and retrying...\n");
		}

		buffer += cc;
		rembytes -= cc;
	}
	return(0);
}

struct slow_arg {
	int size;
	double time;
};

/* returns 0 for success, -1 for failure */
static int
get_slow_args(interp,x)
Tcl_Interp *interp;
struct slow_arg *x;
{
	int sc;		/* return from scanf */
	char *s = exp_get_var(interp,"send_slow");
	if (!s) {
		exp_error(interp,"send -s: send_slow has no value");
		return(-1);
	}
	if (2 != (sc = sscanf(s,"%d %lf",&x->size,&x->time))) {
		exp_error(interp,"send -s: found %d value(s) in send_slow but need 2",sc);
		return(-1);
	}
	if (x->size <= 0) {
		exp_error(interp,"send -s: size (%d) in send_slow must be positive", x->size);
		return(-1);
	}
	if (x->time <= 0) {
		exp_error(interp,"send -s: time (%f) in send_slow must be larger",x->time);
		return(-1);
	}
	return(0);
}

/* returns 0 for success, -1 for failure, pos. for Tcl return value */
static int
slow_write(interp,fd,buffer,rembytes,arg)
Tcl_Interp *interp;
int fd;
char *buffer;
int rembytes;
struct slow_arg *arg;
{
	int rc;

	while (rembytes > 0) {
		int len;

		len = (arg->size<rembytes?arg->size:rembytes);
		if (0 > exact_write(fd,buffer,len)) return(-1);
		rembytes -= arg->size;
		buffer += arg->size;

		/* skip sleep after last write */
		if (rembytes > 0) {
			rc = exp_dsleep(interp,arg->time);
			if (rc>0) return rc;
		}
	}
	return(0);
}

struct human_arg {
	float alpha;		/* average interarrival time in seconds */
	float alpha_eow;	/* as above but for eow transitions */
	float c;		/* shape */
	float min, max;
};

/* returns -1 if error, 0 if success */
static int
get_human_args(interp,x)
Tcl_Interp *interp;
struct human_arg *x;
{
	int sc;		/* return from scanf */
	char *s = exp_get_var(interp,"send_human");

	if (!s) {
		exp_error(interp,"send -h: send_human has no value");
		return(-1);
	}
	if (5 != (sc = sscanf(s,"%f %f %f %f %f",
			&x->alpha,&x->alpha_eow,&x->c,&x->min,&x->max))) {
		if (sc == EOF) sc = 0;	/* make up for overloaded return */
		exp_error(interp,"send -h: found %d value(s) in send_human but need 5",sc);
		return(-1);
	}
	if (x->alpha < 0 || x->alpha_eow < 0) {
		exp_error(interp,"send -h: average interarrival times (%f %f) must be non-negative in send_human", x->alpha,x->alpha_eow);
		return(-1);
	}
	if (x->c <= 0) {
		exp_error(interp,"send -h: variability (%f) in send_human must be positive",x->c);
		return(-1);
	}
	x->c = 1/x->c;

	if (x->min < 0) {
		exp_error(interp,"send -h: minimum (%f) in send_human must be non-negative",x->min);
		return(-1);
	}
	if (x->max < 0) {
		exp_error(interp,"send -h: maximum (%f) in send_human must be non-negative",x->max);
		return(-1);
	}
	if (x->max < x->min) {
		exp_error(interp,"send -h: maximum (%f) must be >= minimum (%f) in send_human",x->max,x->min);
		return(-1);
	}
	return(0);
}

/* Compute random numbers from 0 to 1, for expect's send -h */
/* This implementation sacrifices beauty for portability */
static float
unit_random()
{
	/* current implementation is pathetic but works */
	/* 99991 is largest prime in my CRC - can't hurt, eh? */
	return((float)(1+(rand()%99991))/99991.0);
}

void
exp_init_unit_random()
{
	srand(getpid());
}

/* This function is my implementation of the Weibull distribution. */
/* I've added a max time and an "alpha_eow" that captures the slight */
/* but noticable change in human typists when hitting end-of-word */
/* transitions. */
/* returns 0 for success, -1 for failure, pos. for Tcl return value */
static int
human_write(interp,fd,buffer,arg)
Tcl_Interp *interp;
int fd;
char *buffer;
struct human_arg *arg;
{
	char *sp;
	float t;
	float alpha;
	int wc;
	int in_word = TRUE;

	debuglog("human_write: avg_arr=%f/%f  1/shape=%f  min=%f  max=%f\r\n",
		arg->alpha,arg->alpha_eow,arg->c,arg->min,arg->max);

	for (sp = buffer;*sp;sp++) {
		/* use the end-of-word alpha at eow transitions */
		if (in_word && (ispunct(*sp) || isspace(*sp)))
			alpha = arg->alpha_eow;
		else alpha = arg->alpha;
		in_word = !(ispunct(*sp) || isspace(*sp));

		t = alpha * pow(-log((double)unit_random()),arg->c);

		/* enforce min and max times */
		if (t<arg->min) t = arg->min;
		else if (t>arg->max) t = arg->max;

/*fprintf(stderr,"\nwriting <%c> but first sleep %f seconds\n",*sp,t);*/
		/* skip sleep before writing first character */
		if (sp != buffer) {
			wc = exp_dsleep(interp,(double)t);
			if (wc > 0) return wc;
		}

		wc = write(fd,sp,1);
		if (0 > wc) return(wc);
	}
	return(0);
}

struct exp_i *exp_i_pool = 0;
struct exp_fd_list *exp_fd_list_pool = 0;

#define EXP_I_INIT_COUNT	10
#define EXP_FD_INIT_COUNT	10

struct exp_i *
exp_new_i()
{
	int n;
	struct exp_i *i;

	if (!exp_i_pool) {
		/* none avail, generate some new ones */
		exp_i_pool = i = (struct exp_i *)ckalloc(
			EXP_I_INIT_COUNT * sizeof(struct exp_i));
		for (n=0;n<EXP_I_INIT_COUNT-1;n++,i++) {
			i->next = i+1;
		}
		i->next = 0;
	}

	/* now that we've made some, unlink one and give to user */

	i = exp_i_pool;
	exp_i_pool = exp_i_pool->next;
	i->value = 0;
	i->variable = 0;
	i->fd_list = 0;
	i->ecount = 0;
	i->next = 0;
	return i;
}

struct exp_fd_list *
exp_new_fd(val)
int val;
{
	int n;
	struct exp_fd_list *fd;

	if (!exp_fd_list_pool) {
		/* none avail, generate some new ones */
		exp_fd_list_pool = fd = (struct exp_fd_list *)ckalloc(
			EXP_FD_INIT_COUNT * sizeof(struct exp_fd_list));
		for (n=0;n<EXP_FD_INIT_COUNT-1;n++,fd++) {
			fd->next = fd+1;
		}
		fd->next = 0;
	}

	/* now that we've made some, unlink one and give to user */

	fd = exp_fd_list_pool;
	exp_fd_list_pool = exp_fd_list_pool->next;
	fd->fd = val;
	/* fd->next is assumed to be changed by caller */
	return fd;
}

void
exp_free_fd(fd_first)
struct exp_fd_list *fd_first;
{
	struct exp_fd_list *fd, *penultimate;

	if (!fd_first) return;

	/* link entire chain back in at once by first finding last pointer */
	/* making that point back to pool, and then resetting pool to this */

	/* run to end */
	for (fd = fd_first;fd;fd=fd->next) {
		penultimate = fd;
	}
	penultimate->next = exp_fd_list_pool;
	exp_fd_list_pool = fd_first;
}

/* free a single fd */
void
exp_free_fd_single(fd)
struct exp_fd_list *fd;
{
	fd->next = exp_fd_list_pool;
	exp_fd_list_pool = fd;
}

void
exp_free_i(interp,i,updateproc)
Tcl_Interp *interp;
struct exp_i *i;
Tcl_VarTraceProc *updateproc;	/* proc to invoke if indirect is written */
{
	if (i->next) exp_free_i(interp,i->next,updateproc);

	exp_free_fd(i->fd_list);

	if (i->direct == EXP_INDIRECT) {
		Tcl_UntraceVar(interp,i->variable,
			TCL_GLOBAL_ONLY|TCL_TRACE_WRITES,
			updateproc,(ClientData)i);
	}

	/* here's the long form
	   if duration & direct	free(var)  free(val)
		PERM	  DIR	    		1
		PERM	  INDIR	    1		1
		TMP	  DIR
		TMP	  INDIR			1
	   Also if i->variable was a bogus variable name, i->value might not be
	   set, so test i->value to protect this
	   TMP in this case does NOT mean from the "expect" command.  Rather
	   it means "an implicit spawn id from any expect or expect_XXX
	   command".  In other words, there was no variable name provided.
	*/
	if (i->value
	   && (((i->direct == EXP_DIRECT) && (i->duration == EXP_PERMANENT))
		|| ((i->direct == EXP_INDIRECT) && (i->duration == EXP_TEMPORARY)))) {
		ckfree(i->value);
	} else if (i->duration == EXP_PERMANENT) {
		if (i->value) ckfree(i->value);
		if (i->variable) ckfree(i->variable);
	}

	i->next = exp_i_pool;
	exp_i_pool = i;
}

/* generate a descriptor for a "-i" flag */
/* cannot fail */
struct exp_i *
exp_new_i_complex(interp,arg,duration,updateproc)
Tcl_Interp *interp;
char *arg;		/* spawn id list or a variable containing a list */
int duration;		/* if we have to copy the args */
			/* should only need do this in expect_before/after */
Tcl_VarTraceProc *updateproc;	/* proc to invoke if indirect is written */
{
	struct exp_i *i;
	char **stringp;

	i = exp_new_i();

	i->direct = (isdigit(arg[0]) || (arg[0] == '-'))?EXP_DIRECT:EXP_INDIRECT;
	if (i->direct == EXP_DIRECT) {
		stringp = &i->value;
	} else {
		stringp = &i->variable;
	}

	i->duration = duration;
	if (duration == EXP_PERMANENT) {
		*stringp = ckalloc(strlen(arg)+1);
		strcpy(*stringp,arg);
	} else {
		*stringp = arg;
	}

	i->fd_list = 0;
	exp_i_update(interp,i);

	/* if indirect, ask Tcl to tell us when variable is modified */

	if (i->direct == EXP_INDIRECT) {
		Tcl_TraceVar(interp, i->variable,
			TCL_GLOBAL_ONLY|TCL_TRACE_WRITES,
			updateproc, (ClientData) i);
	}

	return i;
}

void
exp_i_add_fd(i,fd)
struct exp_i *i;
int fd;
{
	struct exp_fd_list *new_fd;

	new_fd = exp_new_fd(fd);
	new_fd->next = i->fd_list;
	i->fd_list = new_fd;
}

/* this routine assumes i->fd is meaningful */
void
exp_i_parse_fds(i)
struct exp_i *i;
{
	char *p = i->value;

	/* reparse it */
	while (1) {
		int m;
		int negative = 0;
		int valid_spawn_id = 0;

		m = 0;
		while (isspace(*p)) p++;
		for (;;p++) {
			if (*p == '-') negative = 1;
			else if (isdigit(*p)) {
				m = m*10 + (*p-'0');
				valid_spawn_id = 1;
			} else if (*p == '\0' || isspace(*p)) break;
		}

		/* we either have a spawn_id or whitespace at end of string */

		/* skip whitespace end-of-string */
		if (!valid_spawn_id) break;

		if (negative) m = -m;

		exp_i_add_fd(i,m);
	}
}
	
/* updates a single exp_i struct */
void
exp_i_update(interp,i)
Tcl_Interp *interp;
struct exp_i *i;
{
	char *p;	/* string representation of list of spawn ids */

	if (i->direct == EXP_INDIRECT) {
		p = Tcl_GetVar(interp,i->variable,TCL_GLOBAL_ONLY);
		if (!p) {
			p = "";
			exp_debuglog("warning: indirect variable %s undefined",i->variable);
		}

		if (i->value) {
			if (streq(p,i->value)) return;

			/* replace new value with old */
			ckfree(i->value);
		}
		i->value = ckalloc(strlen(p)+1);
		strcpy(i->value,p);

		exp_free_fd(i->fd_list);
		i->fd_list = 0;
	} else {
		/* no free, because this should only be called on */
		/* "direct" i's once */
		i->fd_list = 0;
	}
	exp_i_parse_fds(i);
}

struct exp_i *
exp_new_i_simple(fd,duration)
int fd;
int duration;		/* if we have to copy the args */
			/* should only need do this in expect_before/after */
{
	struct exp_i *i;

	i = exp_new_i();

	i->direct = EXP_DIRECT;
	i->duration = duration;

	exp_i_add_fd(i,fd);

	return i;
}

/*ARGSUSED*/
static int
Exp_SendLogCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char *string;
	int len;

	argv++;
	argc--;

	if (argc) {
		if (streq(*argv,"--")) {
			argc--; argv++;
		}
	}

	if (argc != 1) {
		exp_error(interp,"usage: send [args] string");
		return TCL_ERROR;
	}

	string = *argv;

	len = strlen(string);

	if (debugfile) fwrite(string,1,len,debugfile);
	if (logfile) fwrite(string,1,len,logfile);

	return(TCL_OK);
}


/* I've rewritten this to be unbuffered.  I did this so you could shove */
/* large files through "send".  If you are concerned about efficiency */
/* you should quote all your send args to make them one single argument. */
/*ARGSUSED*/
static int
Exp_SendCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int m = -1;	/* spawn id (master) */
	int rc; 	/* final result of this procedure */
	struct human_arg human_args;
	struct slow_arg slow_args;
#define SEND_STYLE_STRING_MASK	0x07	/* mask to detect a real string arg */
#define SEND_STYLE_PLAIN	0x01
#define SEND_STYLE_HUMAN	0x02
#define SEND_STYLE_SLOW		0x04
#define SEND_STYLE_ZERO		0x10
#define SEND_STYLE_BREAK	0x20
	int send_style = SEND_STYLE_PLAIN;
	int want_cooked = TRUE;
	char *string;		/* string to send */
	int len;		/* length of string to send */
	int zeros;		/* count of how many ascii zeros to send */

	char *i_masters = 0;
	struct exp_fd_list *fd;
	struct exp_i *i;
	char *arg;

	argv++;
	argc--;
	while (argc) {
		arg = *argv;
		if (arg[0] != '-') break;
		arg++;
		if (exp_flageq1('-',arg)) {			/* "--" */
			argc--; argv++;
			break;
		} else if (exp_flageq1('i',arg)) {		/* "-i" */
			argc--; argv++;
			if (argc==0) {
				exp_error(interp,"usage: -i spawn_id");
				return(TCL_ERROR);
			}
			i_masters = *argv;
			argc--; argv++;
			continue;
		} else if (exp_flageq1('h',arg)) {		/* "-h" */
			argc--; argv++;
			if (-1 == get_human_args(interp,&human_args))
				return(TCL_ERROR);
			send_style = SEND_STYLE_HUMAN;
			continue;
		} else if (exp_flageq1('s',arg)) {		/* "-s" */
			argc--; argv++;
			if (-1 == get_slow_args(interp,&slow_args))
				return(TCL_ERROR);
			send_style = SEND_STYLE_SLOW;
			continue;
		} else if (exp_flageq("null",arg,1) || exp_flageq1('0',arg)) {
			argc--; argv++;				/* "-null" */
			if (!*argv) zeros = 1;
			else {
				zeros = atoi(*argv);
				argc--; argv++;
				if (zeros < 1) return TCL_OK;
			}
			send_style = SEND_STYLE_ZERO;
			string = "<zero(s)>";
			continue;
		} else if (exp_flageq("raw",arg,1)) {		/* "-raw" */
			argc--; argv++;
			want_cooked = FALSE;
			continue;
		} else if (exp_flageq("break",arg,1)) {		/* "-break" */
			argc--; argv++;
			send_style = SEND_STYLE_BREAK;
			string = "<break>";
			continue;
		} else {
			exp_error(interp,"usage: unrecognized flag <-%.80s>",arg);
			return TCL_ERROR;
		}
	}

	if (send_style & SEND_STYLE_STRING_MASK) {
		if (argc != 1) {
			exp_error(interp,"usage: send [args] string");
			return TCL_ERROR;
		}
		string = *argv;
	}
	len = strlen(string);

	if (clientData == &sendCD_user) m = 1;
	else if (clientData == &sendCD_error) m = 2;
	else if (clientData == &sendCD_tty) m = exp_dev_tty;
	else if (!i_masters) {
		/* we really do want to check if it is open */
		/* but since stdin could be closed, we have to first */
		/* get the fd and then convert it from 0 to 1 if necessary */
		if (0 == exp_update_master(interp,&m,0,0))
			return(TCL_ERROR);
	}

	/* if master != -1, then it holds desired master */
	/* else i_masters does */

	if (m != -1) {
		i = exp_new_i_simple(m,EXP_TEMPORARY);
	} else {
		i = exp_new_i_complex(interp,i_masters,FALSE,(Tcl_VarTraceProc *)0);
	}

#define send_to_stderr	(clientData == &sendCD_error)
#define send_to_proc	(clientData == &sendCD_proc)
#define send_to_user	((clientData == &sendCD_user) || \
			 (clientData == &sendCD_tty))

	if (send_to_proc) {
		want_cooked = FALSE;
		debuglog("send: sending \"%s\" to {",dprintify(string));
		/* if closing brace doesn't appear, that's because an error */
		/* was encountered before we could send it */
	} else {
		if (debugfile)
			fwrite(string,1,len,debugfile);
		if ((send_to_user && logfile_all) || logfile)
			fwrite(string,1,len,logfile);
	}

	for (fd=i->fd_list;fd;fd=fd->next) {
		m = fd->fd;

		if (send_to_proc) {
			debuglog(" %d ",m);
		}

		/* true if called as Send with user_spawn_id */
		if (exp_is_stdinfd(m)) m = 1;

		/* check validity of each - i.e., are they open */
		if (0 == exp_fd2f(interp,m,1,0,"send")) {
			rc = TCL_ERROR;
			goto finish;
		}
		/* Check if Tcl is using a different fd for output */
		if (exp_fs[m].tcl_handle) {
			m = exp_fs[m].tcl_output;
		}

		if (want_cooked) string = exp_cook(string,&len);

		switch (send_style) {
		case SEND_STYLE_PLAIN:
			rc = exact_write(m,string,len);
			break;
		case SEND_STYLE_SLOW:
			rc = slow_write(interp,m,string,len,&slow_args);
			break;
		case SEND_STYLE_HUMAN:
			rc = human_write(interp,m,string,&human_args);
			break;
		case SEND_STYLE_ZERO:
			for (;zeros>0;zeros--) rc = write(m,"",1);
			/* catching error on last write is sufficient */
			rc = ((rc==1) ? 0 : -1);   /* normal is 1 not 0 */
			break;
		case SEND_STYLE_BREAK:
			exp_tty_break(interp,m);
			rc = 0;
			break;
		}

		if (rc != 0) {
			if (rc == -1) {
				exp_error(interp,"write(spawn_id=%d): %s",m,Tcl_PosixError(interp));
				rc = TCL_ERROR;
			}
			goto finish;
		}
	}
	if (send_to_proc) debuglog("}\r\n");

	rc = TCL_OK;
 finish:
	exp_free_i(interp,i,(Tcl_VarTraceProc *)0);
	return rc;
}

/*ARGSUSED*/
static int
Exp_LogFileCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	static Tcl_DString dstring;
	static int first_time = TRUE;
	static int current_append;	/* true if currently appending */
	static char *openarg = 0;	/* Tcl file identifier from -open */
	static int leaveopen = FALSE;	/* true if -leaveopen was used */

	int old_logfile_all = logfile_all;
	FILE *old_logfile = logfile;
	char *old_openarg = openarg;
	int old_leaveopen = leaveopen;

	int aflag = FALSE;
	int append = TRUE;
	char *filename = 0;
	char *type;
	FILE *writefilePtr;
	int usage_error_occurred = FALSE;

	openarg = 0;
	leaveopen = FALSE;

	if (first_time) {
		Tcl_DStringInit(&dstring);
		first_time = FALSE;
	}


#define usage_error	if (0) ; else {\
				 usage_error_occurred = TRUE;\
				 goto error;\
			}

	/* when this function returns, we guarantee that if logfile_all */
	/* is TRUE, then logfile is non-zero */

	argv++;
	argc--;
	for (;argc>0;argc--,argv++) {
		if (streq(*argv,"-open")) {
			if (!argv[1]) usage_error;
			openarg = ckalloc(strlen(argv[1])+1);
			strcpy(openarg,argv[1]);
			argc--; argv++;
		} else if (streq(*argv,"-leaveopen")) {
			if (!argv[1]) usage_error;
			openarg = ckalloc(strlen(argv[1])+1);
			strcpy(openarg,argv[1]);
			leaveopen = TRUE;
			argc--; argv++;
		} else if (streq(*argv,"-a")) {
			aflag = TRUE;
		} else if (streq(*argv,"-info")) {
			if (logfile) {
				if (logfile_all) strcat(interp->result,"-a ");
				if (!current_append) strcat(interp->result,"-noappend ");
				strcat(interp->result,Tcl_DStringValue(&dstring));
			}
			return TCL_OK;
		} else if (streq(*argv,"-noappend")) {
			append = FALSE;
		} else break;
	}

	if (argc == 1) {
		filename = argv[0];
	} else if (argc > 1) {
		/* too many arguments */
		usage_error
	} 

	if (openarg && filename) {
		usage_error
	}
	if (aflag && !(openarg || filename)) {
		usage_error
	}

	logfile = 0;
	logfile_all = aflag;

	current_append = append;

	type = (append?"a":"w");

	if (filename) {
		filename = Tcl_TildeSubst(interp,filename,&dstring);
		if (filename == NULL) {
			goto error;
		} else {
			/* Tcl_TildeSubst doesn't store into dstring */
			/* if no ~, so force string into dstring */
			/* this is only needed so that next time around */
			/* we can get dstring for -info if necessary */
			if (Tcl_DStringValue(&dstring)[0] == '\0') {
				Tcl_DStringAppend(&dstring,filename,-1);
			}
		}

		errno = 0;
		if (NULL == (logfile = fopen(filename,type))) {
			char *msg;

			if (errno == 0) {
				msg = open_failed;
			} else {
				msg = Tcl_PosixError(interp);
			}
			exp_error(interp,"%s: %s",filename,msg);
			Tcl_DStringFree(&dstring);
			goto error;
		}
	} else if (openarg) {
		int cc;
		int fd;
		Tcl_Channel chan;
		int mode;
#if TCL_MAJOR_VERSION < 8
		Tcl_File tclWriteFile;
#endif /* TCL_MAJOR_VERSION < 8 */

		Tcl_DStringTrunc(&dstring,0);

#ifdef __CYGWIN32__
               /* This doesn't work on cygwin32, because
                   Tcl_GetChannelHandle is likely to return a Windows
                   handle, and passing that to dup will fail.  */
               exp_error(interp,"log_file -open and -leaveopen not supported on
 cygwin32");
               return TCL_ERROR;
#endif

#if TCL7_4
		cc = Tcl_GetOpenFile(interp,openarg,1,1,&writefilePtr);
		if (cc == TCL_ERROR) goto error;

		if (-1 == (fd = dup(fileno(writefilePtr)))) {
			exp_error(interp,"dup: %s",Tcl_PosixError(interp));
			goto error;
		}
#endif
		if (!(chan = Tcl_GetChannel(interp,openarg,&mode))) {
			return TCL_ERROR;
		}
		if (!(mode & TCL_WRITABLE)) {
			exp_error(interp,"channel is not writable");
		}
#if TCL_MAJOR_VERSION < 8
		tclWriteFile = Tcl_GetChannelFile(chan, TCL_WRITABLE);
		fd = dup((int)Tcl_GetFileInfo(tclWriteFile, (int *)0));
#else
		if (TCL_ERROR == Tcl_GetChannelHandle(chan, TCL_WRITABLE, (ClientData) &fd)) {
			goto error;
		}
		fd = dup(fd);
#endif /* TCL_MAJOR_VERSION < 8 */
		if (!(logfile = fdopen(fd,type))) {
			exp_error(interp,"fdopen: %s",Tcl_PosixError(interp));
			close(fd);
			goto error;
		}

		if (leaveopen) {
			Tcl_DStringAppend(&dstring,"-leaveopen ",-1);
		} else {
			Tcl_DStringAppend(&dstring,"-open ",-1);
		}

                Tcl_DStringAppend(&dstring,openarg,-1);

		/*
		 * It would be convenient now to tell Tcl to close its
		 * file descriptor.  Alas, if involved in a pipeline, Tcl
		 * will be unable to complete a wait on the process.
		 * So simply remember that we meant to close it.  We will
		 * do so later in our own close routine.
		 */
	}
	if (logfile) {
		setbuf(logfile,(char *)0);
		exp_close_on_exec(fileno(logfile));
	}

	if (old_logfile) {
		fclose(old_logfile);
	}

	if (old_openarg) {
		if (!old_leaveopen) {
			close_tcl_file(interp,old_openarg);
		}
		ckfree((char *)old_openarg);
	}

	return TCL_OK;

 error:
	if (old_logfile) {
		logfile = old_logfile;
		logfile_all = old_logfile_all;
	}

	if (openarg) ckfree(openarg);
	openarg = old_openarg;
	leaveopen = old_leaveopen;

	if (usage_error_occurred) {
		exp_error(interp,"usage: log_file [-info] [-noappend] [[-a] file] [-[leave]open [open ...]]");
	}

	return TCL_ERROR;
}

/*ARGSUSED*/
static int
Exp_LogUserCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int old_loguser = loguser;

	if (argc == 0 || (argc == 2 && streq(argv[1],"-info"))) {
		/* do nothing */
	} else if (argc == 2) {
		if (0 == atoi(argv[1])) loguser = FALSE;
		else loguser = TRUE;
	} else {
		exp_error(interp,"usage: [-info|1|0]");
	}

	sprintf(interp->result,"%d",old_loguser);

	return(TCL_OK);
}

#ifdef TCL_DEBUGGER
/*ARGSUSED*/
static int
Exp_DebugCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int now = FALSE;	/* soon if FALSE, now if TRUE */
	int exp_tcl_debugger_was_available = exp_tcl_debugger_available;

	if (argc > 3) goto usage;

	if (argc == 1) {
		sprintf(interp->result,"%d",exp_tcl_debugger_available);
		return TCL_OK;
	}

	argv++;

	while (*argv) {
		if (streq(*argv,"-now")) {
			now = TRUE;
			argv++;
		}
		else break;
	}

	if (!*argv) {
		if (now) {
			Dbg_On(interp,1);
			exp_tcl_debugger_available = 1;
		} else {
			goto usage;
		}
	} else if (streq(*argv,"0")) {
		Dbg_Off(interp);
		exp_tcl_debugger_available = 0;
	} else {
		Dbg_On(interp,now);
		exp_tcl_debugger_available = 1;
	}
	sprintf(interp->result,"%d",exp_tcl_debugger_was_available);
	return(TCL_OK);
 usage:
	exp_error(interp,"usage: [[-now] 1|0]");
	return TCL_ERROR;
}
#endif

/*ARGSUSED*/
static int
Exp_ExpInternalCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	static Tcl_DString dstring;
	static int first_time = TRUE;
	int fopened = FALSE;

	if (first_time) {
		Tcl_DStringInit(&dstring);
		first_time = FALSE;
	}

	if (argc > 1 && streq(argv[1],"-info")) {
		if (debugfile) {
			sprintf(interp->result,"-f %s ",
				Tcl_DStringValue(&dstring));
		}
		strcat(interp->result,((exp_is_debugging==0)?"0":"1"));
		return TCL_OK;
	}

	argv++;
	argc--;
	while (argc) {
		if (!streq(*argv,"-f")) break;
		argc--;argv++;
		if (argc < 1) goto usage;
		if (debugfile) fclose(debugfile);
		argv[0] = Tcl_TildeSubst(interp, argv[0],&dstring);
		if (argv[0] == NULL) goto error;
		else {
			/* Tcl_TildeSubst doesn't store into dstring */
			/* if no ~, so force string into dstring */
			/* this is only needed so that next time around */
			/* we can get dstring for -info if necessary */
			if (Tcl_DStringValue(&dstring)[0] == '\0') {
				Tcl_DStringAppend(&dstring,argv[0],-1);
			}
		}

		errno = 0;
		if (NULL == (debugfile = fopen(*argv,"a"))) {
			char *msg;

			if (errno == 0) {
				msg = open_failed;
			} else {
				msg = Tcl_PosixError(interp);
			}

			exp_error(interp,"%s: %s",*argv,msg);
			goto error;
		}
		setbuf(debugfile,(char *)0);
		exp_close_on_exec(fileno(debugfile));
		fopened = TRUE;
		argc--;argv++;
	}

	if (argc != 1) goto usage;

	/* if no -f given, close file */
	if (fopened == FALSE && debugfile) {
		fclose(debugfile);
		debugfile = 0;
		Tcl_DStringFree(&dstring);
	}

	exp_is_debugging = atoi(*argv);
	return(TCL_OK);
 usage:
	exp_error(interp,"usage: [-f file] expr");
 error:
	Tcl_DStringFree(&dstring);
	return TCL_ERROR;
}

char *exp_onexit_action = 0;

/*ARGSUSED*/
static int
Exp_ExitCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int value = 0;

	argv++;

	if (*argv) {
		if (exp_flageq(*argv,"-onexit",3)) {
			argv++;
			if (*argv) {
				int len = strlen(*argv);
				if (exp_onexit_action)
					ckfree(exp_onexit_action);
				exp_onexit_action = ckalloc(len + 1);
				strcpy(exp_onexit_action,*argv);
			} else if (exp_onexit_action) {
				Tcl_AppendResult(interp,exp_onexit_action,(char *)0);
			}
			return TCL_OK;
		} else if (exp_flageq(*argv,"-noexit",3)) {
			argv++;
			exp_exit_handlers((ClientData)interp);
			return TCL_OK;
		}
	}

	if (*argv) {
		if (Tcl_GetInt(interp, *argv, &value) != TCL_OK) {
			return TCL_ERROR;
		}
	}

	exp_exit(interp,value);
	/*NOTREACHED*/
}

/* so cmd table later is more intuitive */
#define Exp_CloseObjCmd Exp_CloseCmd

/*ARGSUSED*/
static int
Exp_CloseCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
#if TCL_MAJOR_VERSION < 8
char **argv;
#else
Tcl_Obj *CONST argv[];	/* Argument objects. */
#endif
{
	int onexec_flag = FALSE;	/* true if -onexec seen */
	int close_onexec;
	int slave_flag = FALSE;
	int m = -1;

	int argc_orig = argc;
#if TCL_MAJOR_VERSION < 8
	char **argv_orig = argv;
#else
	Tcl_Obj *CONST *argv_orig = argv;
#endif

	argc--; argv++;

#if TCL_MAJOR_VERSION < 8
#define STARARGV *argv
#else
#define STARARGV Tcl_GetStringFromObj(*argv,(int *)0)
#endif

	for (;argc>0;argc--,argv++) {
		if (streq("-i",STARARGV)) {
			argc--; argv++;
			if (argc == 0) {
				exp_error(interp,"usage: -i spawn_id");
				return(TCL_ERROR);
			}
			m = atoi(STARARGV);
		} else if (streq(STARARGV,"-slave")) {
			slave_flag = TRUE;
		} else if (streq(STARARGV,"-onexec")) {
			argc--; argv++;
			if (argc == 0) {
				exp_error(interp,"usage: -onexec 0|1");
				return(TCL_ERROR);
			}
			onexec_flag = TRUE;
			close_onexec = atoi(STARARGV);
		} else break;
	}

	if (argc) {
		/* doesn't look like our format, it must be a Tcl-style file */
		/* handle.  Lucky that formats are easily distinguishable. */
		/* Historical note: we used "close"  long before there was a */
		/* Tcl builtin by the same name. */

		Tcl_CmdInfo info;
		Tcl_ResetResult(interp);
		if (0 == Tcl_GetCommandInfo(interp,"close",&info)) {
			info.clientData = 0;
		}
#if TCL_MAJOR_VERSION < 8
		return(Tcl_CloseCmd(info.clientData,interp,argc_orig,argv_orig));
#else
		return(Tcl_CloseObjCmd(info.clientData,interp,argc_orig,argv_orig));
#endif
	}

	if (m == -1) {
		if (exp_update_master(interp,&m,1,0) == 0) return(TCL_ERROR);
	}

	if (slave_flag) {
		struct exp_f *f = exp_fd2f(interp,m,1,0,"-slave");
		if (!f) return TCL_ERROR;

		if (f->slave_fd) {
			close(f->slave_fd);
			f->slave_fd = EXP_NOFD;

			exp_slave_control(m,1);

			return TCL_OK;
		} else {
			exp_error(interp,"no such slave");
			return TCL_ERROR;
		}
	}

	if (onexec_flag) {
		/* heck, don't even bother to check if fd is open or a real */
		/* spawn id, nothing else depends on it */
		fcntl(m,F_SETFD,close_onexec);
		return TCL_OK;
	}

	return(exp_close(interp,m));
}

/*ARGSUSED*/
static void
tcl_tracer(clientData,interp,level,command,cmdProc,cmdClientData,argc,argv)
ClientData clientData;
Tcl_Interp *interp;
int level;
char *command;
int (*cmdProc)();
ClientData cmdClientData;
int argc;
char *argv[];
{
	int i;

	/* come out on stderr, by using errorlog */
	errorlog("%2d",level);
	for (i = 0;i<level;i++) exp_nferrorlog("  ",0/*ignored - satisfy lint*/);
	errorlog("%s\r\n",command);
}

/*ARGSUSED*/
static int
Exp_StraceCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	static int trace_level = 0;
	static Tcl_Trace trace_handle;

	if (argc > 1 && streq(argv[1],"-info")) {
		sprintf(interp->result,"%d",trace_level);
		return TCL_OK;
	}

	if (argc != 2) {
		exp_error(interp,"usage: trace level");
		return(TCL_ERROR);
	}
	/* tracing already in effect, undo it */
	if (trace_level > 0) Tcl_DeleteTrace(interp,trace_handle);

	/* get and save new trace level */
	trace_level = atoi(argv[1]);
	if (trace_level > 0)
		trace_handle = Tcl_CreateTrace(interp,
				trace_level,tcl_tracer,(ClientData)0);
	return(TCL_OK);
}

/* following defn's are stolen from tclUnix.h */

/*
 * The type of the status returned by wait varies from UNIX system
 * to UNIX system.  The macro below defines it:
 */

#if 0
#ifndef NO_UNION_WAIT
#   define WAIT_STATUS_TYPE union wait
#else
#   define WAIT_STATUS_TYPE int
#endif
#endif /* 0 */

/*
 * following definitions stolen from tclUnix.h
 * (should have been made public!)

 * Supply definitions for macros to query wait status, if not already
 * defined in header files above.
 */

#if 0
#ifndef WIFEXITED
#   define WIFEXITED(stat)  (((*((int *) &(stat))) & 0xff) == 0)
#endif

#ifndef WEXITSTATUS
#   define WEXITSTATUS(stat) (((*((int *) &(stat))) >> 8) & 0xff)
#endif

#ifndef WIFSIGNALED
#   define WIFSIGNALED(stat) (((*((int *) &(stat)))) && ((*((int *) &(stat))) == ((*((int *) &(stat))) & 0x00ff)))
#endif

#ifndef WTERMSIG
#   define WTERMSIG(stat)    ((*((int *) &(stat))) & 0x7f)
#endif

#ifndef WIFSTOPPED
#   define WIFSTOPPED(stat)  (((*((int *) &(stat))) & 0xff) == 0177)
#endif

#ifndef WSTOPSIG
#   define WSTOPSIG(stat)    (((*((int *) &(stat))) >> 8) & 0xff)
#endif
#endif /* 0 */

/* end of stolen definitions */

/* Describe the processes created with Expect's fork.
This allows us to wait on them later.

This is maintained as a linked list.  As additional procs are forked,
new links are added.  As procs disappear, links are marked so that we
can reuse them later.
*/

struct forked_proc {
	int pid;
	WAIT_STATUS_TYPE wait_status;
	enum {not_in_use, wait_done, wait_not_done} link_status;
	struct forked_proc *next;
} *forked_proc_base = 0;

void
fork_clear_all()
{
	struct forked_proc *f;

	for (f=forked_proc_base;f;f=f->next) {
		f->link_status = not_in_use;
	}
}

void
fork_init(f,pid)
struct forked_proc *f;
int pid;
{
	f->pid = pid;
	f->link_status = wait_not_done;
}

/* make an entry for a new proc */
void
fork_add(pid)
int pid;
{
	struct forked_proc *f;

	for (f=forked_proc_base;f;f=f->next) {
		if (f->link_status == not_in_use) break;
	}

	/* add new entry to the front of the list */
	if (!f) {
		f = (struct forked_proc *)ckalloc(sizeof(struct forked_proc));
		f->next = forked_proc_base;
		forked_proc_base = f;
	}
	fork_init(f,pid);
}

/* Provide a last-chance guess for this if not defined already */
#ifndef WNOHANG
#define WNOHANG WNOHANG_BACKUP_VALUE
#endif

/* wait returns are a hodgepodge of things
 If wait fails, something seriously has gone wrong, for example:
   bogus arguments (i.e., incorrect, bogus spawn id)
   no children to wait on
   async event failed
 If wait succeeeds, something happened on a particular pid
   3rd arg is 0 if successfully reaped (if signal, additional fields supplied)
   3rd arg is -1 if unsuccessfully reaped (additional fields supplied)
*/
/*ARGSUSED*/
static int
Exp_WaitCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int master_supplied = FALSE;
	int m;			/* master waited for */
	struct exp_f *f;	/* ditto */
	struct forked_proc *fp = 0;	/* handle to a pure forked proc */

	struct exp_f ftmp;	/* temporary memory for either f or fp */

	int nowait = FALSE;
	int result = 0;		/* 0 means child was successfully waited on */
				/* -1 means an error occurred */
				/* -2 means no eligible children to wait on */
#define NO_CHILD -2

	argv++;
	argc--;
	for (;argc>0;argc--,argv++) {
		if (streq(*argv,"-i")) {
			argc--; argv++;
			if (argc==0) {
				exp_error(interp,"usage: -i spawn_id");
				return(TCL_ERROR);
			}
			master_supplied = TRUE;
			m = atoi(*argv);
		} else if (streq(*argv,"-nowait")) {
			nowait = TRUE;
		}
	}

	if (!master_supplied) {
		if (0 == exp_update_master(interp,&m,0,0))
			return TCL_ERROR;
	}

	if (m != EXP_SPAWN_ID_ANY) {
		if (0 == exp_fd2f(interp,m,0,0,"wait")) {
			return TCL_ERROR;
		}

		f = exp_fs + m;

		/* check if waited on already */
		/* things opened by "open" or set with -nowait */
		/* are marked sys_waited already */
		if (!f->sys_waited) {
			if (nowait) {
				/* should probably generate an error */
				/* if SIGCHLD is trapped. */

				/* pass to Tcl, so it can do wait */
				/* in background */
#if TCL_MAJOR_VERSION < 8
				Tcl_DetachPids(1,&f->pid);
#else
				Tcl_DetachPids(1,(Tcl_Pid *)&f->pid);
#endif
				exp_wait_zero(&f->wait);
			} else {
				while (1) {
					if (Tcl_AsyncReady()) {
						int rc = Tcl_AsyncInvoke(interp,TCL_OK);
						if (rc != TCL_OK) return(rc);
					}

					result = waitpid(f->pid,&f->wait,0);
					if (result == f->pid) break;
					if (result == -1) {
						if (errno == EINTR) continue;
						else break;
					}
				}
			}
		}

		/*
		 * Now have Tcl reap anything we just detached. 
		 * This also allows procs user has created with "exec &"
		 * and and associated with an "exec &" process to be reaped.
		 */

		Tcl_ReapDetachedProcs();
		exp_rearm_sigchld(interp); /* new */
	} else {
		/* wait for any of our own spawned processes */
		/* we call waitpid rather than wait to avoid running into */
		/* someone else's processes.  Yes, according to Ousterhout */
		/* this is the best way to do it. */

		for (m=0;m<=exp_fd_max;m++) {
			f = exp_fs + m;
			if (!f->valid) continue;
			if (f->pid == exp_getpid) continue; /* skip ourself */
			if (f->user_waited) continue;	/* one wait only! */
			if (f->sys_waited) break;
		   restart:
			result = waitpid(f->pid,&f->wait,WNOHANG);
			if (result == f->pid) break;
			if (result == 0) continue;	/* busy, try next */
			if (result == -1) {
				if (errno == EINTR) goto restart;
				else break;
			}
		}

		/* if it's not a spawned process, maybe its a forked process */
		for (fp=forked_proc_base;fp;fp=fp->next) {
			if (fp->link_status == not_in_use) continue;
		restart2:
			result = waitpid(fp->pid,&fp->wait_status,WNOHANG);
			if (result == fp->pid) {
				m = -1; /* DOCUMENT THIS! */
				break;
			}
			if (result == 0) continue;	/* busy, try next */
			if (result == -1) {
				if (errno == EINTR) goto restart2;
				else break;
			}
		}

		if (m > exp_fd_max) {
			result = NO_CHILD;	/* no children */
			Tcl_ReapDetachedProcs();
		}
		exp_rearm_sigchld(interp);
	}

	/*  sigh, wedge forked_proc into an exp_f structure so we don't
	 *  have to rewrite remaining code (too much)
	 */
	if (fp) {
		f = &ftmp;
		f->pid = fp->pid;
		f->wait = fp->wait_status;
	}

	/* non-portable assumption that pid_t can be printed with %d */

	if (result == -1) {
		sprintf(interp->result,"%d %d -1 %d POSIX %s %s",
			f->pid,m,errno,Tcl_ErrnoId(),Tcl_ErrnoMsg(errno));
		result = TCL_OK;
	} else if (result == NO_CHILD) {
		interp->result = "no children";
		return TCL_ERROR;
	} else {
		sprintf(interp->result,"%d %d 0 %d",
					f->pid,m,WEXITSTATUS(f->wait));
		if (WIFSIGNALED(f->wait)) {
			Tcl_AppendElement(interp,"CHILDKILLED");
			Tcl_AppendElement(interp,Tcl_SignalId((int)(WTERMSIG(f->wait))));
			Tcl_AppendElement(interp,Tcl_SignalMsg((int) (WTERMSIG(f->wait))));
		} else if (WIFSTOPPED(f->wait)) {
			Tcl_AppendElement(interp,"CHILDSUSP");
			Tcl_AppendElement(interp,Tcl_SignalId((int) (WSTOPSIG(f->wait))));
			Tcl_AppendElement(interp,Tcl_SignalMsg((int) (WSTOPSIG(f->wait))));
		}
	}
			
	if (fp) {
		fp->link_status = not_in_use;
		return ((result == -1)?TCL_ERROR:TCL_OK);		
	}

	f->sys_waited = TRUE;
	f->user_waited = TRUE;

	/* if user has already called close, make sure fd really is closed */
	/* and forget about this entry entirely */
	if (f->user_closed) {
		if (!f->sys_closed) {
			sys_close(m,f);
		}
		f->valid = FALSE;
	}
	return ((result == -1)?TCL_ERROR:TCL_OK);
}

/*ARGSUSED*/
static int
Exp_ForkCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int rc;
	if (argc > 1) {
		exp_error(interp,"usage: fork");
		return(TCL_ERROR);
	}

	rc = fork();
	if (rc == -1) {
		exp_error(interp,"fork: %s",Tcl_PosixError(interp));
		return TCL_ERROR;
	} else if (rc == 0) {
		/* child */
		exp_forked = TRUE;
		exp_getpid = getpid();
		fork_clear_all();
	} else {
		/* parent */
		fork_add(rc);
	}

	/* both child and parent follow remainder of code */
	sprintf(interp->result,"%d",rc);
	debuglog("fork: returns {%s}\r\n",interp->result);
	return(TCL_OK);
}

/*ARGSUSED*/
static int
Exp_DisconnectCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	/* tell Saber to ignore non-use of ttyfd */
	/*SUPPRESS 591*/
	int ttyfd;

	if (argc > 1) {
		exp_error(interp,"usage: disconnect");
		return(TCL_ERROR);
	}

	if (exp_disconnected) {
		exp_error(interp,"already disconnected");
		return(TCL_ERROR);
	}
	if (!exp_forked) {
		exp_error(interp,"can only disconnect child process");
		return(TCL_ERROR);
	}
	exp_disconnected = TRUE;

	/* ignore hangup signals generated by testing ptys in getptymaster */
	/* and other places */
	signal(SIGHUP,SIG_IGN);

	/* reopen prevents confusion between send/expect_user */
	/* accidentally mapping to a real spawned process after a disconnect */
	if (exp_fs[0].pid != EXP_NOPID) {
		exp_close(interp,0);
		open("/dev/null",0);
		fd_new(0, EXP_NOPID);
	}
	if (exp_fs[1].pid != EXP_NOPID) {
		exp_close(interp,1);
		open("/dev/null",1);
		fd_new(1, EXP_NOPID);
	}
	if (exp_fs[2].pid != EXP_NOPID) {
		/* reopen stderr saves error checking in error/log routines. */
		exp_close(interp,2);
		open("/dev/null",1);
		fd_new(2, EXP_NOPID);
	}

	Tcl_UnsetVar(interp,"tty_spawn_id",TCL_GLOBAL_ONLY);

#ifdef DO_SETSID
	setsid();
#else
#ifdef SYSV3
	/* put process in our own pgrp, and lose controlling terminal */
#ifdef sysV88
	/* With setpgrp first, child ends up with closed stdio */
	/* according to Dave Schmitt <daves@techmpc.csg.gss.mot.com> */
	if (fork()) exit(0);
	setpgrp();
#else
	setpgrp();
	/*signal(SIGHUP,SIG_IGN); moved out to above */
	if (fork()) exit(0);	/* first child exits (as per Stevens, */
	/* UNIX Network Programming, p. 79-80) */
	/* second child process continues as daemon */
#endif
#else /* !SYSV3 */
#ifdef MIPS_BSD
	/* required on BSD side of MIPS OS <jmsellen@watdragon.waterloo.edu> */
#	include <sysv/sys.s>
	syscall(SYS_setpgrp);
#endif
	setpgrp(0,0);
/*	setpgrp(0,getpid());*/	/* put process in our own pgrp */

/* Pyramid lacks this defn */
#ifdef TIOCNOTTY
	ttyfd = open("/dev/tty", O_RDWR);
	if (ttyfd >= 0) {
		/* zap controlling terminal if we had one */
		(void) ioctl(ttyfd, TIOCNOTTY, (char *)0);
		(void) close(ttyfd);
	}
#endif /* TIOCNOTTY */

#endif /* SYSV3 */
#endif /* DO_SETSID */
	return(TCL_OK);
}

/*ARGSUSED*/
static int
Exp_OverlayCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int newfd, oldfd;
	int dash_name = 0;
	char *command;

	argc--; argv++;
	while (argc) {
		if (*argv[0] != '-') break;	/* not a flag */
		if (streq(*argv,"-")) {		/* - by itself */
			argc--; argv++;
			dash_name = 1;
			continue;
		}
		newfd = atoi(argv[0]+1);
		argc--; argv++;
		if (argc == 0) {
			exp_error(interp,"overlay -# requires additional argument");
			return(TCL_ERROR);
		}
		oldfd = atoi(argv[0]);
		argc--; argv++;
		debuglog("overlay: mapping fd %d to %d\r\n",oldfd,newfd);
		if (oldfd != newfd) (void) dup2(oldfd,newfd);
		else debuglog("warning: overlay: old fd == new fd (%d)\r\n",oldfd);
	}
	if (argc == 0) {
		exp_error(interp,"need program name");
		return(TCL_ERROR);
	}
	command = argv[0];
	if (dash_name) {
		argv[0] = ckalloc(1+strlen(command));
		sprintf(argv[0],"-%s",command);
	}

	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
        (void) execvp(command,argv);
	exp_error(interp,"execvp(%s): %s\r\n",argv[0],Tcl_PosixError(interp));
	return(TCL_ERROR);
}

#if 0
/*ARGSUSED*/
int
cmdReady(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char num[4];	/* can hold up to "999 " */
	char buf[1024];	/* can easily hold 256 spawn_ids! */
	int i, j;
	int *masters, *masters2;
	int timeout = get_timeout();

	if (argc < 2) {
		exp_error(interp,"usage: ready spawn_id1 [spawn_id2 ...]");
		return(TCL_ERROR);
	}

	masters = (int *)ckalloc((argc-1)*sizeof(int));
	masters2 = (int *)ckalloc((argc-1)*sizeof(int));

	for (i=1;i<argc;i++) {
		j = atoi(argv[i]);
		if (!exp_fd2f(interp,j,1,"ready")) {
			ckfree(masters);
			return(TCL_ERROR);
		}
		masters[i-1] = j;
	}
	j = i-1;
	if (TCL_ERROR == ready(masters,i-1,masters2,&j,&timeout))
		return(TCL_ERROR);

	/* pack result back into out-array */
	buf[0] = '\0';
	for (i=0;i<j;i++) {
		sprintf(num,"%d ",masters2[i]); /* note extra blank */
		strcat(buf,num);
	}
	ckfree(masters); ckfree(masters2);
	Tcl_Return(interp,buf,TCL_VOLATILE);
	return(TCL_OK);
}
#endif

/*ARGSUSED*/
int
Exp_InterpreterCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	if (argc != 1) {
		exp_error(interp,"no arguments allowed");
		return(TCL_ERROR);
	}

	return(exp_interpreter(interp));
	/* errors and ok, are caught by exp_interpreter() and discarded */
	/* to return TCL_OK, type "return" */
}

/* this command supercede's Tcl's builtin CONTINUE command */
/*ARGSUSED*/
int
Exp_ExpContinueDeprecatedCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
       if (argc == 1) return(TCL_CONTINUE);
       else if (argc == 2) {
               if (streq(argv[1],"-expect")) {
                       debuglog("continue -expect is deprecated, use exp_continue\r\n");
                       return(EXP_CONTINUE);
               }
       }
       exp_error(interp,"usage: continue [-expect]\n");
       return(TCL_ERROR);
}

/* this command supercede's Tcl's builtin CONTINUE command */
/*ARGSUSED*/
int
Exp_ExpContinueCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	if (argc == 1) {
		return EXP_CONTINUE;
	} else if ((argc == 2) && (0 == strcmp(argv[1],"-continue_timer"))) {
		return EXP_CONTINUE_TIMER;
	}

	exp_error(interp,"usage: exp_continue [-continue_timer]\n");
	return(TCL_ERROR);
}

#if TCL_MAJOR_VERSION < 8
/* most of this is directly from Tcl's definition for return */
/*ARGSUSED*/
int
Exp_InterReturnCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	/* let Tcl's return command worry about args */
	/* if successful (i.e., TCL_RETURN is returned) */
	/* modify the result, so that we will handle it specially */

	int result = Tcl_ReturnCmd(clientData,interp,argc,argv);
	if (result == TCL_RETURN)
		result = EXP_TCL_RETURN;
	return result;
}
#else
/* most of this is directly from Tcl's definition for return */
/*ARGSUSED*/
int
Exp_InterReturnObjCmd(clientData, interp, objc, objv)
ClientData clientData;
Tcl_Interp *interp;
int objc;
Tcl_Obj *CONST objv[];
{
    /* let Tcl's return command worry about args */
    /* if successful (i.e., TCL_RETURN is returned) */
    /* modify the result, so that we will handle it specially */

#if TCL_MAJOR_VERSION < 8
    int result = Tcl_ReturnCmd(clientData,interp,objc,objv);
#else
       int result = Tcl_ReturnObjCmd(clientData,interp,objc,objv);
#endif

    if (result == TCL_RETURN)
        result = EXP_TCL_RETURN;
    return result;
}
#endif

/*ARGSUSED*/
int
Exp_OpenCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct exp_f *f;
	int m = -1;
	int m2;
	int leaveopen = FALSE;
	Tcl_Channel chan;

	argc--; argv++;

	for (;argc>0;argc--,argv++) {
		if (streq(*argv,"-i")) {
			argc--; argv++;
			if (!*argv) {
				exp_error(interp,"usage: -i spawn_id");
				return TCL_ERROR;
			}
			m = atoi(*argv);
		} else if (streq(*argv,"-leaveopen")) {
			leaveopen = TRUE;
			argc--; argv++;
		} else break;
	}

	if (m == -1) {
		if (exp_update_master(interp,&m,0,0) == 0) return TCL_ERROR;
	}

	if (0 == (f = exp_fd2f(interp,m,1,0,"exp_open"))) return TCL_ERROR;

	/* make a new copy of file descriptor */
	if (-1 == (m2 = dup(m))) {
		exp_error(interp,"fdopen: %s",Tcl_PosixError(interp));
		return TCL_ERROR;
	}

	if (!leaveopen) {
		/* remove from Expect's memory in anticipation of passing to Tcl */
		if (f->pid != EXP_NOPID) {
#if TCL_MAJOR_VERSION < 8
			Tcl_DetachPids(1,&f->pid);
#else
			Tcl_DetachPids(1,(Tcl_Pid *)&f->pid);
#endif
			f->pid = EXP_NOPID;
			f->sys_waited = f->user_waited = TRUE;
		}
		exp_close(interp,m);
	}

	chan = Tcl_MakeFileChannel(
#if TCL_MAJOR_VERSION < 8
			    (ClientData)m2,
#endif
			    (ClientData)m2,
			    TCL_READABLE|TCL_WRITABLE);
	Tcl_RegisterChannel(interp, chan);
	Tcl_AppendResult(interp, Tcl_GetChannelName(chan), (char *) NULL);
	return TCL_OK;
}

/* return 1 if a string is substring of a flag */
/* this version is the code used by the macro that everyone calls */
int
exp_flageq_code(flag,string,minlen)
char *flag;
char *string;
int minlen;		/* at least this many chars must match */
{
	for (;*flag;flag++,string++,minlen--) {
		if (*string == '\0') break;
		if (*string != *flag) return 0;
	}
	if (*string == '\0' && minlen <= 0) return 1;
	return 0;
}

void
exp_create_commands(interp,c)
Tcl_Interp *interp;
struct exp_cmd_data *c;
{
#if TCL_MAJOR_VERSION < 8
	Interp *iPtr = (Interp *) interp;
#else
	Namespace *globalNsPtr = (Namespace *) Tcl_GetGlobalNamespace(interp);
	Namespace *currNsPtr   = (Namespace *) Tcl_GetCurrentNamespace(interp);
#endif
	char cmdnamebuf[80];

	for (;c->name;c++) {
#if TCL_MAJOR_VERSION < 8
		int create = FALSE;
		/* if already defined, don't redefine */
		if (c->flags & EXP_REDEFINE) create = TRUE;
                else if (!Tcl_FindHashEntry(&iPtr->commandTable,c->name)) {
			create = TRUE;
		}
		if (create) {
				Tcl_CreateCommand(interp,c->name,c->proc,
						  c->data,exp_deleteProc);
			}
#else
		/* if already defined, don't redefine */
		if ((c->flags & EXP_REDEFINE) ||
		    !(Tcl_FindHashEntry(&globalNsPtr->cmdTable,c->name) ||
		      Tcl_FindHashEntry(&currNsPtr->cmdTable,c->name))) {
			if (c->objproc)
				Tcl_CreateObjCommand(interp,c->name,
						     c->objproc,c->data,exp_deleteObjProc);
			else
			Tcl_CreateCommand(interp,c->name,c->proc,
					  c->data,exp_deleteProc);
		}
#endif
		if (!(c->name[0] == 'e' &&
		      c->name[1] == 'x' &&
		      c->name[2] == 'p')
		    && !(c->flags & EXP_NOPREFIX)) {
			sprintf(cmdnamebuf,"exp_%s",c->name);
#if TCL_MAJOR_VERSION < 8
			Tcl_CreateCommand(interp,cmdnamebuf,c->proc,
				c->data,exp_deleteProc);
#else
			if (c->objproc)
				Tcl_CreateObjCommand(interp,cmdnamebuf,c->objproc,c->data,
						     exp_deleteObjProc);
			else
			Tcl_CreateCommand(interp,cmdnamebuf,c->proc,
					     c->data,exp_deleteProc);
#endif
		}
	}
}

static struct exp_cmd_data cmd_data[]  = {
#if TCL_MAJOR_VERSION < 8
{"close",	Exp_CloseCmd,	0,	EXP_REDEFINE},
#else
{"close",	Exp_CloseObjCmd,	0,	0,	EXP_REDEFINE},
#endif
#ifdef TCL_DEBUGGER
{"debug",	exp_proc(Exp_DebugCmd),	0,	0},
#endif
{"exp_internal",exp_proc(Exp_ExpInternalCmd),	0,	0},
{"disconnect",	exp_proc(Exp_DisconnectCmd),	0,	0},
{"exit",	exp_proc(Exp_ExitCmd),	0,	EXP_REDEFINE},
{"exp_continue",exp_proc(Exp_ExpContinueCmd),0,	0},
{"fork",	exp_proc(Exp_ForkCmd),	0,	0},
{"exp_pid",	exp_proc(Exp_ExpPidCmd),	0,	0},
{"getpid",	exp_proc(Exp_GetpidDeprecatedCmd),0,	0},
{"interpreter",	exp_proc(Exp_InterpreterCmd),	0,	0},
{"log_file",	exp_proc(Exp_LogFileCmd),	0,	0},
{"log_user",	exp_proc(Exp_LogUserCmd),	0,	0},
{"exp_open",	exp_proc(Exp_OpenCmd),	0,	0},
{"overlay",	exp_proc(Exp_OverlayCmd),	0,	0},
#if TCL_MAJOR_VERSION < 8
{"inter_return",Exp_InterReturnCmd,	0,	0},
#else
{"inter_return",Exp_InterReturnObjCmd,	0,	0,	0},
#endif
{"send",	exp_proc(Exp_SendCmd),	(ClientData)&sendCD_proc,	0},
{"send_error",	exp_proc(Exp_SendCmd),	(ClientData)&sendCD_error,	0},
{"send_log",	exp_proc(Exp_SendLogCmd),	0,	0},
{"send_tty",	exp_proc(Exp_SendCmd),	(ClientData)&sendCD_tty,	0},
{"send_user",	exp_proc(Exp_SendCmd),	(ClientData)&sendCD_user,	0},
{"sleep",	exp_proc(Exp_SleepCmd),	0,	0},
{"spawn",	exp_proc(Exp_SpawnCmd),	0,	0},
{"strace",	exp_proc(Exp_StraceCmd),	0,	0},
{"wait",	exp_proc(Exp_WaitCmd),	0,	0},
{0}};

void
exp_init_most_cmds(interp)
Tcl_Interp *interp;
{
	exp_create_commands(interp,cmd_data);

#ifdef HAVE_PTYTRAP
	Tcl_InitHashTable(&slaveNames,TCL_STRING_KEYS);
#endif /* HAVE_PTYTRAP */

	exp_close_in_child = exp_close_tcl_files;
}
/* cribbed directly from tclBasic.c */
int
Tcl_CloseCmd(stuff, interp, argc, argv)
     ClientData *stuff;
     Tcl_Interp *interp;
     int argc;
     char **argv;
{
#define NUM_ARGS 20
    Tcl_Obj *(argStorage[NUM_ARGS]);
    register Tcl_Obj **objv = argStorage;
    int i, result;
    Tcl_Obj *objPtr;

    /*
     * Create the object argument array "objv". Make sure objv is large
     * enough to hold the objc arguments plus 1 extra for the zero
     * end-of-objv word.
     */

    if ((argc + 1) > NUM_ARGS) {
        objv = (Tcl_Obj **)
            Tcl_Alloc((unsigned)(argc + 1) * sizeof(Tcl_Obj *));
    }

    for (i = 0;  i < argc;  i++) {
        objPtr = Tcl_NewStringObj(argv[i], -1);
        Tcl_IncrRefCount(objPtr);
        objv[i] = objPtr;
    }
    objv[argc] = 0;

    /*
     * Invoke the command's object-based Tcl_ObjCmdProc.
     */

    result = Tcl_CloseObjCmd(stuff, interp, argc, objv);

    /*
     * Move the interpreter's object result to the string result, 
     * then reset the object result.
     * FAILS IF OBJECT RESULT'S STRING REPRESENTATION CONTAINS NULL BYTES.
     */

    Tcl_SetResult(interp,
            TclGetStringFromObj(Tcl_GetObjResult(interp), (int *) NULL),
            TCL_VOLATILE);
    
    /*
     * Decrement the ref counts for the argument objects created above,
     * then free the objv array if malloc'ed storage was used.
     */

    for (i = 0;  i < argc;  i++) {
        objPtr = objv[i];
        Tcl_DecrRefCount(objPtr);
    }
    if (objv != argStorage) {
        Tcl_Free((char *) objv);
    }
    return result;
#undef NUM_ARGS
}
