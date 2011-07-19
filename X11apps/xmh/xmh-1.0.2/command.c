/* $XConsortium: command.c,v 2.49 95/04/05 19:59:06 kaleb Exp $ */
/* $XFree86: xc/programs/xmh/command.c,v 3.8 2001/12/09 15:48:36 herrb Exp $ */

/*
 *			  COPYRIGHT 1987, 1989
 *		   DIGITAL EQUIPMENT CORPORATION
 *		       MAYNARD, MASSACHUSETTS
 *			ALL RIGHTS RESERVED.
 *
 * THE INFORMATION IN THIS SOFTWARE IS SUBJECT TO CHANGE WITHOUT NOTICE AND
 * SHOULD NOT BE CONSTRUED AS A COMMITMENT BY DIGITAL EQUIPMENT CORPORATION.
 * DIGITAL MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THIS SOFTWARE FOR
 * ANY PURPOSE.  IT IS SUPPLIED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 *
 * IF THE SOFTWARE IS MODIFIED IN A MANNER CREATING DERIVATIVE COPYRIGHT
 * RIGHTS, APPROPRIATE LEGENDS MAY BE PLACED ON THE DERIVATIVE WORK IN
 * ADDITION TO THAT SET FORTH ABOVE.
 *
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Digital Equipment Corporation not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 */

/* command.c -- interface to exec mh commands. */

#include "xmh.h"
#include <X11/Xpoll.h>
#include <sys/ioctl.h>
#include <signal.h>
#ifndef SYSV
#include <sys/wait.h>
#endif	/* SYSV */
#if defined(SVR4) && !defined(DGUX)
#include <sys/filio.h>
#endif

/* number of user input events to queue before malloc */
#define TYPEAHEADSIZE 20

#ifndef HAS_VFORK
#define vfork() fork()
#else
#if defined(sun) && !defined(SVR4)
#include <vfork.h>
#endif
#endif

typedef struct _CommandStatus {
    Widget	popup;		 /* must be first; see PopupStatus */
    struct _LastInput lastInput; /* must be second; ditto */
    char*	shell_command;	 /* must be third; for XmhShellCommand */
    int		child_pid;
    XtInputId	output_inputId;
    XtInputId	error_inputId;
    int		output_pipe[2];
    int		error_pipe[2];
    char*	output_buffer;
    int		output_buf_size;
    char*	error_buffer;
    int		error_buf_size;
} CommandStatusRec, *CommandStatus;

typedef char* Pointer;
static void FreeStatus(XMH_CB_ARGS);
static void CheckReadFromPipe(int, char **, int *, Bool);

static void SystemError(char* text)
{
    char msg[BUFSIZ];
    sprintf( msg, "%s; errno = %d %s", text, errno, 
	     strerror(errno));
    XtWarning( msg );
}


/* Return the full path name of the given mh command. */

static char *FullPathOfCommand(char *str)
{
    static char result[100];
    (void) sprintf(result, "%s/%s", app_resources.mh_path, str);
    return result;
}


/*ARGSUSED*/
static void ReadStdout(
    XtPointer closure,
    int *fd,
    XtInputId *id)	/* unused */
{
    register CommandStatus status = (CommandStatus)closure;
    CheckReadFromPipe(*fd, &status->output_buffer, &status->output_buf_size,
		      False);
}


/*ARGSUSED*/
static void ReadStderr(
    XtPointer closure,
    int *fd,
    XtInputId *id)	/* unused */
{
    register CommandStatus status = (CommandStatus)closure;
    CheckReadFromPipe(*fd, &status->error_buffer, &status->error_buf_size,
		      False);
}


static volatile int childdone;		/* Gets nonzero when the child process
				   finishes. */
/* ARGSUSED */
static void
ChildDone(int n)
{
    childdone++;
}

/* Execute the given command, and wait until it has finished.  While the
   command is executing, watch the X socket and cause Xlib to read in any
   incoming data.  This will prevent the socket from overflowing during
   long commands.  Returns 0 if stderr empty, -1 otherwise. */

static int _DoCommandToFileOrPipe(
  char **argv,			/* The command to execute, and its args. */
  int inputfd,			/* Input stream for command. */
  int outputfd,			/* Output stream; /dev/null if == -1 */
  char **bufP,			/* output buffer ptr if outputfd == -2 */
  int *lenP)			/* output length ptr if outputfd == -2 */
{
    XtAppContext appCtx = XtWidgetToApplicationContext(toplevel);
    int return_status;
    int old_stdin = 0, old_stdout = 0, old_stderr = 0;
    int pid;
    fd_set readfds, fds;
    Boolean output_to_pipe = False;
    CommandStatus status = XtNew(CommandStatusRec);
    FD_ZERO(&fds);
    FD_SET(ConnectionNumber(theDisplay), &fds);
    DEBUG1("Executing %s ...", argv[0])

    if (inputfd != -1) {
	old_stdin = dup(fileno(stdin));
	(void) dup2(inputfd, fileno(stdin));
	close(inputfd);
    }

    if (outputfd == -1) {
	if (!app_resources.debug) { /* Throw away stdout. */
	    outputfd = open( "/dev/null", O_WRONLY, 0 );
	}
    }
    else if (outputfd == -2) {	/* make pipe */
	if (pipe(status->output_pipe) /*failed*/) {
	    SystemError( "couldn't re-direct standard output" );
	    status->output_pipe[0]=0;
	}
	else {
	    outputfd = status->output_pipe[1];
	    FD_SET(status->output_pipe[0], &fds);
	    status->output_inputId =
		XtAppAddInput( appCtx,
			   status->output_pipe[0], (XtPointer)XtInputReadMask,
			   ReadStdout, (XtPointer)status
			     );
	    status->output_buffer = NULL;
	    status->output_buf_size = 0;
	    output_to_pipe = True;
	}
    }

    if (pipe(status->error_pipe) /*failed*/) {
	SystemError( "couldn't re-direct standard error" );
	status->error_pipe[0]=0;
    }
    else {
	old_stderr = dup(fileno(stderr));
	(void) dup2(status->error_pipe[1], fileno(stderr));
	close(status->error_pipe[1]);
	FD_SET(status->error_pipe[0], &fds);
	status->error_inputId =
	    XtAppAddInput( appCtx,
			   status->error_pipe[0], (XtPointer)XtInputReadMask,
			   ReadStderr, (XtPointer)status
			  );
    }
    if (outputfd != -1) {
	old_stdout = dup(fileno(stdout));
	(void) dup2(outputfd, fileno(stdout));
	close(outputfd);
    }
    childdone = FALSE;
    status->popup = (Widget)NULL;
    status->lastInput = lastInput;
    status->error_buffer = NULL;
    status->error_buf_size = 0;
    (void) signal(SIGCHLD, ChildDone);
    pid = vfork();
    if (inputfd != -1) {
	if (pid != 0) dup2(old_stdin,  fileno(stdin));
	close(old_stdin);
    }
    if (outputfd != -1) {
	if (pid != 0) dup2(old_stdout, fileno(stdout));
	close(old_stdout);
    }
    if (status->error_pipe[0]) {
	if (pid != 0) dup2(old_stderr, fileno(stderr));
	close(old_stderr);
    }

    if (pid == -1) Punt("Couldn't fork!");
    if (pid) {			/* We're the parent process. */
	XEvent typeAheadQueue[TYPEAHEADSIZE], *eventP = typeAheadQueue;
	XEvent *altQueue = NULL;
	int type_ahead_count = 0, alt_queue_size = 0, alt_queue_count = 0;
	XtAppContext app = XtWidgetToApplicationContext(toplevel);
	int num_fds = ConnectionNumber(theDisplay)+1;
	if (output_to_pipe && status->output_pipe[0] >= num_fds)
	    num_fds = status->output_pipe[0]+1;
	if (status->error_pipe[0] >= num_fds)
	    num_fds = status->error_pipe[0]+1;
	status->child_pid = pid;
	DEBUG1( " pid=%d ", pid )
	subProcessRunning = True;
	while (!childdone) {
	    while (!(XtAppPending(app) & XtIMXEvent)) {
		/* this is gross, but the only other way is by
		 * polling on timers or an extra pipe, since we're not
		 * guaranteed to be able to malloc in a signal handler.
		 */
		readfds = fds;
                if (childdone) break;
		DEBUG("blocking.\n")
		(void) Select(num_fds, &readfds, NULL, NULL, NULL);
		DEBUG1("unblocked; child%s done.\n", childdone ? "" : " not")
		if (childdone) break;
		if (!FD_ISSET(ConnectionNumber(theDisplay), &readfds)) {
		    DEBUG("reading alternate input...")
		    XtAppProcessEvent(appCtx, (unsigned)XtIMAlternateInput);
		    DEBUG("read.\n")
		}
	    }
	    if (childdone) break;
	    XtAppNextEvent(app, eventP);
	    switch(eventP->type) {
	      case LeaveNotify:
		if (type_ahead_count) {
		    /* do compress_enterleave here to save memory */
		    XEvent *prevEvent;
		    if (alt_queue_size && (alt_queue_count == 0))
			prevEvent = &typeAheadQueue[type_ahead_count-1];
		    else
			prevEvent = eventP - 1;
		    if (prevEvent->type == EnterNotify
		      && prevEvent->xany.display == eventP->xany.display
		      && prevEvent->xany.window == eventP->xany.window) {
			eventP = prevEvent;
			if (alt_queue_count > 0)
			    alt_queue_count--;
			else
			    type_ahead_count--;
			break;
		    }
		}
		/* fall through */
	      case KeyPress:
	      case KeyRelease:
	      case EnterNotify:
	      case ButtonPress:
	      case ButtonRelease:
	      case MotionNotify:
		if (type_ahead_count < TYPEAHEADSIZE) {
		    if (++type_ahead_count == TYPEAHEADSIZE) {
			altQueue = (XEvent*)XtMalloc(
				(Cardinal)TYPEAHEADSIZE*sizeof(XEvent) );     
			alt_queue_size = TYPEAHEADSIZE;
			eventP = altQueue;
		    }
		    else
			eventP++;
		}
		else {
		    if (++alt_queue_count == alt_queue_size) {
			alt_queue_size += TYPEAHEADSIZE;
			altQueue = (XEvent*)XtRealloc(
				(char*)altQueue,
				(Cardinal)alt_queue_size*sizeof(XEvent) );
			eventP = &altQueue[alt_queue_count];
		    }
		    else
			eventP++;
		}
		break;

	      default:
		XtDispatchEvent(eventP);
	    }
	}
	(void) wait(0);

	DEBUG("done\n")
	subProcessRunning = False;
	if (output_to_pipe) {
	    CheckReadFromPipe( status->output_pipe[0],
			       &status->output_buffer,
			       &status->output_buf_size,
			       True
			      );
	    *bufP = status->output_buffer;
	    *lenP = status->output_buf_size;
	    close( status->output_pipe[0] );
	    XtRemoveInput( status->output_inputId );
	}
	if (status->error_pipe[0]) {
	    CheckReadFromPipe( status->error_pipe[0],
			       &status->error_buffer,
			       &status->error_buf_size,
			       True
			      );
	    close( status->error_pipe[0] );
	    XtRemoveInput( status->error_inputId );
	}
	if (status->error_buffer != NULL) {
	    /* special case for arbitrary shell commands: capture command */
	    if ((strcmp(argv[0], "/bin/sh") == 0) &&
	        (strcmp(argv[1], "-c") == 0)) {
	        status->shell_command = XtNewString(argv[2]);
            } else status->shell_command = (char*) NULL;
	
	    while (status->error_buffer[status->error_buf_size-1]  == '\0')
		status->error_buf_size--;
	    while (status->error_buffer[status->error_buf_size-1]  == '\n')
		status->error_buffer[--status->error_buf_size] = '\0';
	    DEBUG1( "stderr = \"%s\"\n", status->error_buffer )
	    PopupNotice( status->error_buffer, FreeStatus, (Pointer)status );
	    return_status = -1;
	}
	else {
	    XtFree( (Pointer)status );
	    return_status = 0;
	}
	for (;alt_queue_count;alt_queue_count--) {
	    XPutBackEvent(theDisplay, --eventP);
	}
	if (type_ahead_count) {
	    if (alt_queue_size) eventP = &typeAheadQueue[type_ahead_count];
	    for (;type_ahead_count;type_ahead_count--) {
		XPutBackEvent(theDisplay, --eventP);
	    }
	}
    } else {			/* We're the child process. */
	/* take it from the user's path, else fall back to the mhPath */
	(void) execvp(argv[0], argv);
	(void) execv(FullPathOfCommand(argv[0]), argv);
        progName = argv[0];	/* for Punt message */
	Punt("(cannot execvp it)");
	return_status = -1;
    }
    return return_status;
}


static void
CheckReadFromPipe(
    int fd,
    char **bufP,
    int *lenP,
    Bool waitEOF)
{
    int nread;
/*  DEBUG2( " CheckReadFromPipe #%d,len=%d,", fd, *lenP )  */
#ifdef FIONREAD
    if (!ioctl( fd, FIONREAD, &nread )) {
/*      DEBUG1( "nread=%d ...", nread )			   */
	if (nread) {
	    int old_end = *lenP;
	    *bufP = XtRealloc( *bufP, (Cardinal) ((*lenP += nread) + 1) );
	    read( fd, *bufP+old_end, nread );
	    (*bufP)[old_end+nread] = '\0';
	}
	return;
    }
#endif
    do {
	char buf[512];
	int old_end = *lenP;
	nread = read( fd, buf, 512 );
	if (nread <= 0)
	    break;
	*bufP = XtRealloc( *bufP, (Cardinal) ((*lenP += nread) + 1) );
	memmove( *bufP+old_end, buf, (int) nread );
	(*bufP)[old_end+nread] = '\0';
    } while (waitEOF);
}


/* ARGSUSED */
static void FreeStatus(
    Widget w,			/* unused */
    XtPointer closure,
    XtPointer call_data)	/* unused */
{
    CommandStatus status = (CommandStatus)closure;
    if (status->popup != (Widget)NULL) {
	XtPopdown( status->popup );
	XtDestroyWidget( status->popup );
    }
    if (status->error_buffer != NULL) XtFree(status->error_buffer);
    XtFree( closure );
}

/* Execute the given command, waiting until it's finished.  Put the output
   in the specified file path.  Returns 0 if stderr empty, -1 otherwise */

int DoCommand(
  char **argv,			/* The command to execute, and its args. */
  char *inputfile,		/* Input file for command. */
  char *outputfile)		/* Output file for command. */
{
    int fd_in, fd_out;
    int status;

    if (inputfile != NULL) {
	FILEPTR file = FOpenAndCheck(inputfile, "r");
	fd_in = dup(fileno(file));
	myfclose(file);
    }
    else
	fd_in = -1;

    if (outputfile) {
	FILEPTR file = FOpenAndCheck(outputfile, "w");
	fd_out = dup(fileno(file));
	myfclose(file);
    }
    else
	fd_out = -1;

    status = _DoCommandToFileOrPipe( argv, fd_in, fd_out, (char **) NULL,
				    (int *) NULL );
    return status;
}

/* Execute the given command, waiting until it's finished.  Put the output
   in a newly mallocced string, and return a pointer to that string. */

char *DoCommandToString(char ** argv)
{
    char *result = NULL;
    int len = 0;
    _DoCommandToFileOrPipe( argv, -1, -2, &result, &len );
    if (result == NULL) result = XtMalloc((Cardinal) 1);
    result[len] = '\0';
    DEBUG1("('%s')\n", result)
    return result;
}
    

/* Execute the command to a temporary file, and return the name of the file. */

char *DoCommandToFile(char **argv)
{
    char *name;
    FILEPTR file;
    int fd;
    name = MakeNewTempFileName();
    file = FOpenAndCheck(name, "w");
    fd = dup(fileno(file));
    myfclose(file);
    _DoCommandToFileOrPipe(argv, -1, fd, (char **) NULL, (int *) NULL);
    return name;
}
