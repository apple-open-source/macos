/* exp_poll.c - This file contains UNIX specific procedures for
 * poll-based notifier, which is the lowest-level part of the Tcl
 * event loop.  This file works together with ../generic/tclNotify.c.
 *
 * Design and implementation of this program was paid for by U.S. tax
 * dollars.  Therefore it is public domain.  However, the author and
 * NIST would appreciate credit if this program or parts of it are
 * used.
 *
 * Written by Don Libes, NIST, 2/6/90
 * Rewritten by Don Libes, 2/96 for new Tcl notifier paradigm.
 * Rewritten again by Don Libes, 8/97 for yet another Tcl notifier paradigm.
 */

#include "tclInt.h"
#include "tclPort.h"
#include <signal.h> 

#include <poll.h>
#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

/* Some systems require that the poll array be non-empty so provide a
 * 1-elt array for starters.  It will be ignored as soon as it grows
 * larger.
 */

static struct pollfd initialFdArray;
static struct pollfd *fdArray = &initialFdArray;
static int fdsInUse = 0;	/* space in use */
static int fdsMaxSpace = 1;	/* space that has actually been allocated */

#if TCL_MAJOR_VERSION >= 8

/*
 * tclUnixNotify.c --
 *
 *	This file contains the implementation of the select-based
 *	Unix-specific notifier, which is the lowest-level part of the
 *	Tcl event loop.  This file works together with
 *	../generic/tclNotify.c.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUnixNotfy.c 1.42 97/07/02 20:55:44
 */

/*
 * This structure is used to keep track of the notifier info for a 
 * a registered file.
 */

typedef struct FileHandler {
    int fd;
    int mask;			/* Mask of desired events: TCL_READABLE,
				 * etc. */
    int readyMask;		/* Mask of events that have been seen since the
				 * last time file handlers were invoked for
				 * this file. */
    Tcl_FileProc *proc;		/* Procedure to call, in the style of
				 * Tcl_CreateFileHandler. */
    ClientData clientData;	/* Argument to pass to proc. */
    int pollArrayIndex;		/* index into poll array */
    struct FileHandler *nextPtr;/* Next in list of all files we care about. */
} FileHandler;

/*
 * The following structure is what is added to the Tcl event queue when
 * file handlers are ready to fire.
 */

typedef struct FileHandlerEvent {
    Tcl_Event header;		/* Information that is standard for
				 * all events. */
    int fd;			/* File descriptor that is ready.  Used
				 * to find the FileHandler structure for
				 * the file (can't point directly to the
				 * FileHandler structure because it could
				 * go away while the event is queued). */
} FileHandlerEvent;

/*
 * The following static structure contains the state information for the
 * select based implementation of the Tcl notifier.
 */

static struct {
    FileHandler *firstFileHandlerPtr;
				/* Pointer to head of file handler list. */
    fd_mask checkMasks[3*MASK_SIZE];
				/* This array is used to build up the masks
				 * to be used in the next call to select.
				 * Bits are set in response to calls to
				 * Tcl_CreateFileHandler. */
    fd_mask readyMasks[3*MASK_SIZE];
				/* This array reflects the readable/writable
				 * conditions that were found to exist by the
				 * last call to select. */
    int numFdBits;		/* Number of valid bits in checkMasks
				 * (one more than highest fd for which
				 * Tcl_WatchFile has been called). */
} notifier;

/*
 * The following static indicates whether this module has been initialized.
 */

static int initialized = 0;

/*
 * Static routines defined in this file.
 */

static void		InitNotifier _ANSI_ARGS_((void));
static void		NotifierExitHandler _ANSI_ARGS_((
			    ClientData clientData));
static int		FileHandlerEventProc _ANSI_ARGS_((Tcl_Event *evPtr,
			    int flags));

/*
 *----------------------------------------------------------------------
 *
 * InitNotifier --
 *
 *	Initializes the notifier state.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates a new exit handler.
 *
 *----------------------------------------------------------------------
 */

static void
InitNotifier()
{
    initialized = 1;
    memset(&notifier, 0, sizeof(notifier));
    Tcl_CreateExitHandler(NotifierExitHandler, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * NotifierExitHandler --
 *
 *	This function is called to cleanup the notifier state before
 *	Tcl is unloaded.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys the notifier window.
 *
 *----------------------------------------------------------------------
 */

static void
NotifierExitHandler(clientData)
    ClientData clientData;		/* Not used. */
{
    initialized = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetTimer --
 *
 *	This procedure sets the current notifier timer value.  This
 *	interface is not implemented in this notifier because we are
 *	always running inside of Tcl_DoOneEvent.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetTimer(timePtr)
    Tcl_Time *timePtr;		/* Timeout value, may be NULL. */
{
    /*
     * The interval timer doesn't do anything in this implementation,
     * because the only event loop is via Tcl_DoOneEvent, which passes
     * timeout values to Tcl_WaitForEvent.
     */
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateFileHandler --
 *
 *	This procedure registers a file handler with the Xt notifier.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates a new file handler structure and registers one or more
 *	input procedures with Xt.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_CreateFileHandler(fd, mask, proc, clientData)
    int fd;			/* Handle of stream to watch. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions under which
				 * proc should be called. */
    Tcl_FileProc *proc;		/* Procedure to call for each
				 * selected event. */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    FileHandler *filePtr;
    int index, bit;
    int cur_fd_index;
    
    if (!initialized) {
	InitNotifier();
    }

    for (filePtr = notifier.firstFileHandlerPtr; filePtr != NULL;
	    filePtr = filePtr->nextPtr) {
	if (filePtr->fd == fd) {
	    break;
	}
    }
    if (filePtr == NULL) {
	filePtr = (FileHandler*) ckalloc(sizeof(FileHandler)); /* MLK */
	filePtr->fd = fd;
	filePtr->readyMask = 0;
	filePtr->nextPtr = notifier.firstFileHandlerPtr;
	notifier.firstFileHandlerPtr = filePtr;
    }
    filePtr->proc = proc;
    filePtr->clientData = clientData;
#if NOTUSED
    /* remaining junk is left over from select implementation - DEL */

    filePtr->mask = mask;

    /*
     * Update the check masks for this file.
     */

    index = fd/(NBBY*sizeof(fd_mask));
    bit = 1 << (fd%(NBBY*sizeof(fd_mask)));
    if (mask & TCL_READABLE) {
	notifier.checkMasks[index] |= bit;
    } else {
	notifier.checkMasks[index] &= ~bit;
    } 
    if (mask & TCL_WRITABLE) {
	(notifier.checkMasks+MASK_SIZE)[index] |= bit;
    } else {
	(notifier.checkMasks+MASK_SIZE)[index] &= ~bit;
    }
    if (mask & TCL_EXCEPTION) {
	(notifier.checkMasks+2*(MASK_SIZE))[index] |= bit;
    } else {
	(notifier.checkMasks+2*(MASK_SIZE))[index] &= ~bit;
    }
    if (notifier.numFdBits <= fd) {
	notifier.numFdBits = fd+1;
    }
#endif /* notused */

    filePtr->pollArrayIndex = fdsInUse;
    cur_fd_index = fdsInUse;

    fdsInUse++;
    if (fdsInUse > fdsMaxSpace) {
	if (fdArray != &initialFdArray) ckfree((char *)fdArray);
	fdArray = (struct pollfd *)ckalloc(fdsInUse*sizeof(struct pollfd));
	fdsMaxSpace = fdsInUse;
    }

    fdArray[cur_fd_index].fd = fd;

    /* I know that POLLIN/OUT is right.  But I have no idea if POLLPRI
     * corresponds well to TCL_EXCEPTION.
     */

    if (mask & TCL_READABLE) {
        fdArray[cur_fd_index].events = POLLIN;
    }
    if (mask & TCL_WRITABLE) {
        fdArray[cur_fd_index].events = POLLOUT;
    }
    if (mask & TCL_EXCEPTION) {
        fdArray[cur_fd_index].events = POLLPRI;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteFileHandler --
 *
 *	Cancel a previously-arranged callback arrangement for
 *	a file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If a callback was previously registered on file, remove it.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteFileHandler(fd)
    int fd;		/* Stream id for which to remove callback procedure. */
{
    FileHandler *filePtr, *prevPtr, *lastPtr;
    int index, bit, mask, i;
    int cur_fd_index;

    if (!initialized) {
	InitNotifier();
    }

    /*
     * Find the entry for the given file (and return if there
     * isn't one).
     */

    for (prevPtr = NULL, filePtr = notifier.firstFileHandlerPtr; ;
	    prevPtr = filePtr, filePtr = filePtr->nextPtr) {
	if (filePtr == NULL) {
	    return;
	}
	if (filePtr->fd == fd) {
	    break;
	}
    }

#if NOTUSED
    /* remaining junk is left over from select implementation - DEL */

    /*
     * Update the check masks for this file.
     */

    index = fd/(NBBY*sizeof(fd_mask));
    bit = 1 << (fd%(NBBY*sizeof(fd_mask)));

    if (filePtr->mask & TCL_READABLE) {
	notifier.checkMasks[index] &= ~bit;
    }
    if (filePtr->mask & TCL_WRITABLE) {
	(notifier.checkMasks+MASK_SIZE)[index] &= ~bit;
    }
    if (filePtr->mask & TCL_EXCEPTION) {
	(notifier.checkMasks+2*(MASK_SIZE))[index] &= ~bit;
    }

    /*
     * Find current max fd.
     */

    if (fd+1 == notifier.numFdBits) {
	for (notifier.numFdBits = 0; index >= 0; index--) {
	    mask = notifier.checkMasks[index]
		| (notifier.checkMasks+MASK_SIZE)[index]
		| (notifier.checkMasks+2*(MASK_SIZE))[index];
	    if (mask) {
		for (i = (NBBY*sizeof(fd_mask)); i > 0; i--) {
		    if (mask & (1 << (i-1))) {
			break;
		    }
		}
		notifier.numFdBits = index * (NBBY*sizeof(fd_mask)) + i;
		break;
	    }
	}
    }
#endif /* notused */

    /*
     * Clean up information in the callback record.
     */

    if (prevPtr == NULL) {
	notifier.firstFileHandlerPtr = filePtr->nextPtr;
    } else {
	prevPtr->nextPtr = filePtr->nextPtr;
    }

    /* back to poll-specific code - DEL */

    cur_fd_index = filePtr->pollArrayIndex;
    fdsInUse--;

    /* if this one is last, do nothing special */
    /* else swap with one at end of array */

    if (cur_fd_index != fdsInUse) {
	int lastfd_in_array = fdArray[fdsInUse].fd;
	memcpy(&fdArray[cur_fd_index],&fdArray[fdsInUse],sizeof(struct pollfd));

	/* update index to reflect new location in array */
	/* first find link corresponding to last element in array */
	    
	for (lastPtr = notifier.firstFileHandlerPtr; filePtr; lastPtr = lastPtr->nextPtr) {
	    if (lastPtr->fd == lastfd_in_array) {
		lastPtr->pollArrayIndex = cur_fd_index;
		break;
	    }
	}
    }

    fdsInUse--;

    ckfree((char *) filePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FileHandlerEventProc --
 *
 *	This procedure is called by Tcl_ServiceEvent when a file event
 *	reaches the front of the event queue.  This procedure is
 *	responsible for actually handling the event by invoking the
 *	callback for the file handler.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed
 *	from the queue.  Returns 0 if the event was not handled, meaning
 *	it should stay on the queue.  The only time the event isn't
 *	handled is if the TCL_FILE_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the file handler's callback procedure does.
 *
 *----------------------------------------------------------------------
 */

static int
FileHandlerEventProc(evPtr, flags)
    Tcl_Event *evPtr;		/* Event to service. */
    int flags;			/* Flags that indicate what events to
				 * handle, such as TCL_FILE_EVENTS. */
{
    FileHandler *filePtr;
    FileHandlerEvent *fileEvPtr = (FileHandlerEvent *) evPtr;
    int mask;

    if (!(flags & TCL_FILE_EVENTS)) {
	return 0;
    }

    /*
     * Search through the file handlers to find the one whose handle matches
     * the event.  We do this rather than keeping a pointer to the file
     * handler directly in the event, so that the handler can be deleted
     * while the event is queued without leaving a dangling pointer.
     */

    for (filePtr = notifier.firstFileHandlerPtr; filePtr != NULL;
	    filePtr = filePtr->nextPtr) {
	if (filePtr->fd != fileEvPtr->fd) {
	    continue;
	}

	/*
	 * The code is tricky for two reasons:
	 * 1. The file handler's desired events could have changed
	 *    since the time when the event was queued, so AND the
	 *    ready mask with the desired mask.
	 * 2. The file could have been closed and re-opened since
	 *    the time when the event was queued.  This is why the
	 *    ready mask is stored in the file handler rather than
	 *    the queued event:  it will be zeroed when a new
	 *    file handler is created for the newly opened file.
	 */

	mask = filePtr->readyMask & filePtr->mask;
	filePtr->readyMask = 0;
	if (mask != 0) {
	    (*filePtr->proc)(filePtr->clientData, mask);
	}
	break;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitForEvent --
 *
 *	This function is called by Tcl_DoOneEvent to wait for new
 *	events on the message queue.  If the block time is 0, then
 *	Tcl_WaitForEvent just polls without blocking.
 *
 * Results:
 *	Returns -1 if the select would block forever, otherwise
 *	returns 0.
 *
 * Side effects:
 *	Queues file events that are detected by the select.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WaitForEvent(timePtr)
    Tcl_Time *timePtr;		/* Maximum block time, or NULL. */
{
    FileHandler *filePtr;
    FileHandlerEvent *fileEvPtr;
#if 0
    struct timeval timeout, *timeoutPtr;
#endif
    int timeout;
    struct timeval *timeoutPtr;

    int bit, index, mask, numFound;

    if (!initialized) {
	InitNotifier();
    }

    /*
     * Set up the timeout structure.  Note that if there are no events to
     * check for, we return with a negative result rather than blocking
     * forever.
     */

    if (timePtr) {
#if 0
	timeout.tv_sec = timePtr->sec;
	timeout.tv_usec = timePtr->usec;
	timeoutPtr = &timeout;
#endif
        timeout = timePtr->sec*1000 + timePtr->usec/1000;

    } else if (notifier.numFdBits == 0) {
	return -1;
    } else {
	timeoutPtr = NULL;
    }

    numFound = poll(fdArray,fdsInUse,timeout);
#if 0
    memcpy((VOID *) notifier.readyMasks, (VOID *) notifier.checkMasks,
	    3*MASK_SIZE*sizeof(fd_mask));
    numFound = select(notifier.numFdBits,
	    (SELECT_MASK *) &notifier.readyMasks[0],
	    (SELECT_MASK *) &notifier.readyMasks[MASK_SIZE],
	    (SELECT_MASK *) &notifier.readyMasks[2*MASK_SIZE], timeoutPtr);

    /*
     * Some systems don't clear the masks after an error, so
     * we have to do it here.
     */

    if (numFound == -1) {
	memset((VOID *) notifier.readyMasks, 0, 3*MASK_SIZE*sizeof(fd_mask));
    }
#endif

    /*
     * Queue all detected file events before returning.
     */

    for (filePtr = notifier.firstFileHandlerPtr;
	    (filePtr != NULL) && (numFound > 0);
	    filePtr = filePtr->nextPtr) {
	index = filePtr->pollArrayIndex;
        mask = 0;

        if (fdArray[index].revents & POLLIN) {
	    mask |= TCL_READABLE;
        }
        if (fdArray[index].revents & POLLOUT) {
	    mask |= TCL_WRITABLE;
        }
        /* I have no idea if this is right ... */
        if (fdArray[index].revents & (POLLPRI|POLLERR|POLLHUP|POLLNVAL)) {
	    mask |= TCL_EXCEPTION;
        }

#if 0
	index = filePtr->fd / (NBBY*sizeof(fd_mask));
	bit = 1 << (filePtr->fd % (NBBY*sizeof(fd_mask)));
	mask = 0;

	if (notifier.readyMasks[index] & bit) {
	    mask |= TCL_READABLE;
	}
	if ((notifier.readyMasks+MASK_SIZE)[index] & bit) {
	    mask |= TCL_WRITABLE;
	}
	if ((notifier.readyMasks+2*(MASK_SIZE))[index] & bit) {
	    mask |= TCL_EXCEPTION;
	}
#endif

	if (!mask) {
	    continue;
	} else {
	    numFound--;
	}

	/*
	 * Don't bother to queue an event if the mask was previously
	 * non-zero since an event must still be on the queue.
	 */

	if (filePtr->readyMask == 0) {
	    fileEvPtr = (FileHandlerEvent *) ckalloc(
		sizeof(FileHandlerEvent));
	    fileEvPtr->header.proc = FileHandlerEventProc;
	    fileEvPtr->fd = filePtr->fd;
	    Tcl_QueueEvent((Tcl_Event *) fileEvPtr, TCL_QUEUE_TAIL);
	}
	filePtr->readyMask = mask;
    }
    return 0;
}

#else /* TCL_MAJOR_VERSION < 8 */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WatchFile --
 *
 *	Arrange for Tcl_DoOneEvent to include this file in the masks
 *	for the next call to select.  This procedure is invoked by
 *	event sources, which are in turn invoked by Tcl_DoOneEvent
 *	before it invokes select.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	
 *	The notifier will generate a file event when the I/O channel
 *	given by fd next becomes ready in the way indicated by mask.
 *	If fd is already registered then the old mask will be replaced
 *	with the new one.  Once the event is sent, the notifier will
 *	not send any more events about the fd until the next call to
 *	Tcl_NotifyFile. 
 *
 * Assumption for poll implementation: Tcl_WatchFile is presumed NOT
 * to be called on the same file descriptior without intervening calls
 * to Tcl_DoOneEvent.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_WatchFile(file, mask)
    Tcl_File file;	/* Generic file handle for a stream. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions to wait for
				 * in select. */
{
    int fd, type;
    int cur_fd_index = fdsInUse;

    fd = (int) Tcl_GetFileInfo(file, &type);

    if (type != TCL_UNIX_FD) {
	panic("Tcl_WatchFile: unexpected file type");
    }

    fdsInUse++;
    if (fdsInUse > fdsMaxSpace) {
	if (fdArray != &initialFdArray) ckfree((char *)fdArray);
	fdArray = (struct pollfd *)ckalloc(fdsInUse*sizeof(struct pollfd));
	fdsMaxSpace = fdsInUse;
    }

    fdArray[cur_fd_index].fd = fd;

    /* I know that POLLIN/OUT is right.  But I have no idea if POLLPRI
     * corresponds well to TCL_EXCEPTION.
     */

    if (mask & TCL_READABLE) {
        fdArray[cur_fd_index].events = POLLIN;
    }
    if (mask & TCL_WRITABLE) {
        fdArray[cur_fd_index].events = POLLOUT;
    }
    if (mask & TCL_EXCEPTION) {
        fdArray[cur_fd_index].events = POLLPRI;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_FileReady --
 *
 *	Indicates what conditions (readable, writable, etc.) were
 *	present on a file the last time the notifier invoked select.
 *	This procedure is typically invoked by event sources to see
 *	if they should queue events.
 *
 * Results:
 *	The return value is 0 if none of the conditions specified by mask
 *	was true for fd the last time the system checked.  If any of the
 *	conditions were true, then the return value is a mask of those
 *	that were true.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_FileReady(file, mask)
    Tcl_File file;	/* Generic file handle for a stream. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions caller cares about. */
{
    int index, result, type, fd;
    fd_mask bit;

    fd = (int) Tcl_GetFileInfo(file, &type);
    if (type != TCL_UNIX_FD) {
	panic("Tcl_FileReady: unexpected file type");
    }

    result = 0;
    if ((mask & TCL_READABLE) && (fdArray[fd].revents & POLLIN)) {
	result |= TCL_READABLE;
    }
    if ((mask & TCL_WRITABLE) && (fdArray[fd].revents & POLLOUT)) {
	result |= TCL_WRITABLE;
    }
    /* I have no idea if this is right ... */
    if ((mask & TCL_EXCEPTION) &&
		(fdArray[fd].revents & (POLLPRI|POLLERR|POLLHUP|POLLNVAL))) {
	result |= TCL_EXCEPTION;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitForEvent --
 *
 *	This procedure does the lowest level wait for events in a
 *	platform-specific manner.  It uses information provided by
 *	previous calls to Tcl_WatchFile, plus the timePtr argument,
 *	to determine what to wait for and how long to wait.
 *
 * Results:
 * 7.6  The return value is normally TCL_OK.  However, if there are
 *      no events to wait for (e.g. no files and no timers) so that
 *      the procedure would block forever, then it returns TCL_ERROR.
 *
 * Side effects:
 *	May put the process to sleep for a while, depending on timePtr.
 *	When this procedure returns, an event of interest to the application
 *	has probably, but not necessarily, occurred.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WaitForEvent(timePtr)
    Tcl_Time *timePtr;		/* Specifies the maximum amount of time
				 * that this procedure should block before
				 * returning.  The time is given as an
				 * interval, not an absolute wakeup time.
				 * NULL means block forever. */
{
    int timeout;
    struct timeval *timeoutPtr;

    /* no need to clear revents */
    if (timePtr == NULL) {
	if (!fdsInUse) return (TCL_ERROR);
	timeout = -1;
    } else {
        timeout = timePtr->sec*1000 + timePtr->usec/1000;
    }

    poll(fdArray,fdsInUse,timeout);

    fdsInUse = 0;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Sleep --
 *
 *	Delay execution for the specified number of milliseconds.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Time passes.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Sleep(ms)
    int ms;			/* Number of milliseconds to sleep. */
{
    static struct timeval delay;
    Tcl_Time before, after;

    /*
     * The only trick here is that select appears to return early
     * under some conditions, so we have to check to make sure that
     * the right amount of time really has elapsed.  If it's too
     * early, go back to sleep again.
     */

    TclGetTime(&before);
    after = before;
    after.sec += ms/1000;
    after.usec += (ms%1000)*1000;
    if (after.usec > 1000000) {
	after.usec -= 1000000;
	after.sec += 1;
    }
    while (1) {
	delay.tv_sec = after.sec - before.sec;
	delay.tv_usec = after.usec - before.usec;
	if (delay.tv_usec < 0) {
	    delay.tv_usec += 1000000;
	    delay.tv_sec -= 1;
	}

	/*
	 * Special note:  must convert delay.tv_sec to int before comparing
	 * to zero, since delay.tv_usec is unsigned on some platforms.
	 */

	if ((((int) delay.tv_sec) < 0)
		|| ((delay.tv_usec == 0) && (delay.tv_sec == 0))) {
	    break;
	}

	/* poll understands milliseconds, sigh */
	poll(fdArray,0,delay.tv_sec*1000 + delay.tv_usec/1000);
	TclGetTime(&before);
    }
}

#endif /* TCL_MAJOR_VERSION < 8 */

