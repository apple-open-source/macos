/*
 * fifo.c --
 *
 *	Implementation of a memory channel having fifo behaviour.
 *
 * Copyright (C) 1996-1999 Andreas Kupries (a.kupries@westend.com)
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL I BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS
 * SOFTWARE AND ITS DOCUMENTATION, EVEN IF I HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * I SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND
 * I HAVE NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
 * ENHANCEMENTS, OR MODIFICATIONS.
 *
 * CVS: $Id: fifo.c,v 1.14 2004/11/09 23:11:00 patthoyts Exp $
 */


#include "memchanInt.h"
#include "buf.h"

/*
 * Forward declarations of internal procedures.
 */

static int	Close _ANSI_ARGS_((ClientData instanceData,
		   Tcl_Interp *interp));

static int	Input _ANSI_ARGS_((ClientData instanceData,
		    char *buf, int toRead, int *errorCodePtr));

static int	Output _ANSI_ARGS_((ClientData instanceData,
	            CONST84 char *buf, int toWrite, int *errorCodePtr));

static void	WatchChannel _ANSI_ARGS_((ClientData instanceData, int mask));

static int	GetOption _ANSI_ARGS_((ClientData instanceData,
				       Tcl_Interp* interp,
				       CONST84 char *optionName,
				       Tcl_DString *dsPtr));

static void	ChannelReady _ANSI_ARGS_((ClientData instanceData));
static int      GetFile      _ANSI_ARGS_((ClientData instanceData,
					  int direction,
					  ClientData* handlePtr));

static int	BlockMode _ANSI_ARGS_((ClientData instanceData,
				       int mode));
/*
 * This structure describes the channel type structure for in-memory channels:
 * Fifo are not seekable. They have no writable options, but a readable.
 */

static Tcl_ChannelType channelType = {
  "memory/fifo",	/* Type name.                                    */
  (Tcl_ChannelTypeVersion)BlockMode, /* Set blocking behaviour.          */
  Close,		/* Close channel, clean instance data            */
  Input,		/* Handle read request                           */
  Output,		/* Handle write request                          */
  NULL,			/* Move location of access point.      NULL'able */
  NULL,			/* Set options.                        NULL'able */
  GetOption,		/* Get options.                        NULL'able */
  WatchChannel,		/* Initialize notifier                           */
#if GT81
  GetFile,              /* Get OS handle from the channel.               */
  NULL                  /* Close2Proc, not available, no partial close
			 * possible */
#else
  GetFile               /* Get OS handle from the channel.               */
#endif
};

/*
 * This structure describes the per-instance state of a in-memory fifo channel.
 */

typedef struct ChannelInstance {
  Tcl_Channel    chan;   /* Backreference to generic channel information */
  long int       length; /* Total number of bytes in the channel */

  Buf_BufferQueue queue;  /* Queue of buffers holding the information in this
			   * channel. */

  Tcl_TimerToken timer;  /* Timer used to link the channel into the
			  * notifier. */
  int          interest; /* Interest in events as signaled by the user of
			  * the channel */

#if 0
#if (GT81) && defined (TCL_THREADS)
  Tcl_Mutex lock;        /* Semaphor to handle thread-spanning access to this
			  * fifo. */
#endif
#endif /* 0 */

} ChannelInstance;

/* Macro to check a fifo channel for emptiness.
 */

#define FIFO_EMPTY(c) (c->length == 0)


/*
 *----------------------------------------------------------------------
 *
 * BlockMode --
 *
 *	Helper procedure to set blocking and nonblocking modes on a
 *	memory channel. Invoked by generic IO level code.
 *
 * Results:
 *	0 if successful, errno when failed.
 *
 * Side effects:
 *	Sets the device into blocking or non-blocking mode.
 *
 *----------------------------------------------------------------------
 */

static int
BlockMode (instanceData, mode)
     ClientData instanceData;
     int mode;
{
    return 0;
}

/*
 *------------------------------------------------------*
 *
 *	Close --
 *
 *	------------------------------------------------*
 *	This procedure is called from the generic IO
 *	level to perform channel-type-specific cleanup
 *	when an in-memory fifo channel is closed.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Closes the device of the channel.
 *
 *	Result:
 *		0 if successful, errno if failed.
 *
 *------------------------------------------------------*
 */
/* ARGSUSED */
static int
Close (instanceData, interp)
ClientData  instanceData;    /* The instance information of the channel to
			      * close */
Tcl_Interp* interp;          /* unused */
{
  ChannelInstance* chan;

  chan = (ChannelInstance*) instanceData;

  /* Release the allocated memory. We can be sure that this is done
   * only after the last user closed the channel, i.e. there will be
   * no thread-spanning access !
   */

  if (chan->timer != (Tcl_TimerToken) NULL) {
    Tcl_DeleteTimerHandler (chan->timer);
  }
  chan->timer = (Tcl_TimerToken) NULL;

  Buf_FreeQueue (chan->queue);
  Tcl_Free      ((char*) chan);

  return 0;
}

/*
 *------------------------------------------------------*
 *
 *	Input --
 *
 *	------------------------------------------------*
 *	This procedure is invoked from the generic IO
 *	level to read input from an in-memory fifo channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Reads input from the input device of the
 *		channel.
 *
 *	Result:
 *		The number of bytes read is returned or
 *		-1 on error. An output argument contains
 *		a POSIX error code if an error occurs, or
 *		zero.
 *
 *------------------------------------------------------*
 */

static int
Input (instanceData, buf, toRead, errorCodePtr)
ClientData instanceData;	/* The channel to read from */
char*      buf;			/* Buffer to fill */
int        toRead;		/* Requested number of bytes */
int*       errorCodePtr;	/* Location of error flag */
{
  ChannelInstance* chan;

  if (toRead == 0) {
    return 0;
  }

  chan = (ChannelInstance*) instanceData;

  if (chan->length == 0) {
    /* Signal EOF to higher layer */
    return 0;
  }

  toRead        = Buf_QueueRead (chan->queue, buf, toRead);
  chan->length -= toRead;
  *errorCodePtr = 0;

  return toRead;
}

/*
 *------------------------------------------------------*
 *
 *	Output --
 *
 *	------------------------------------------------*
 *	This procedure is invoked from the generic IO
 *	level to write output to a file channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Writes output on the output device of
 *		the channel.
 *
 *	Result:
 *		The number of bytes written is returned
 *		or -1 on error. An output argument
 *		contains a POSIX error code if an error
 *		occurred, or zero.
 *
 *------------------------------------------------------*
 */

static int
Output (instanceData, buf, toWrite, errorCodePtr)
ClientData instanceData;	/* The channel to write to */
CONST84 char* buf;		/* Data to be stored. */
int        toWrite;		/* Number of bytes to write. */
int*       errorCodePtr;	/* Location of error flag. */
{
  ChannelInstance* chan;

  if (toWrite == 0) {
    return 0;
  }

  chan          = (ChannelInstance*) instanceData;
  toWrite       = Buf_QueueWrite (chan->queue, buf, toWrite);
  chan->length += toWrite;

  return toWrite;
}

/*
 *------------------------------------------------------*
 *
 *	GetOption --
 *
 *	------------------------------------------------*
 *	Computes an option value for a in-memory fifo
 *	channel, or a list of all options and their values.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		A standard Tcl result. The value of the
 *		specified option or a list of all options
 *		and their values is returned in the
 *		supplied DString.
 *
 *------------------------------------------------------*
 */

static int
GetOption (instanceData, interp, optionName, dsPtr)
ClientData   instanceData;	/* Channel to query */
Tcl_Interp*  interp;		/* Interpreter to leave error messages in */
CONST84 char* optionName;	/* Name of reuqested option */
Tcl_DString* dsPtr;		/* String to place the result into */
{
  /*
   * In-memory fifo channels provide two channel type specific,
   * read-only, fconfigure options, "length", that obtains
   * the current number of bytes of data stored in the channel,
   * and "allocated", that obtains the current number of bytes
   * really allocated by the system for its buffers.
   */

  ChannelInstance* chan;
  char             buffer [50];
  /* sufficient even for 64-bit quantities */

  chan = (ChannelInstance*) instanceData;

  /* Known options:
   * -length:    Number of bytes currently used by the buffers.
   * -allocated: Number of bytes currently allocated by the buffers.
   */

  if ((optionName != (char*) NULL) &&
      (0 != strcmp (optionName, "-length")) &&
      (0 != strcmp (optionName, "-allocated"))) {
    Tcl_SetErrno (EINVAL);
    return Tcl_BadChannelOption (interp, optionName, "length allocated");
  }

  if (optionName == (char*) NULL) {
    /*
     * optionName == NULL
     * => a list of options and their values was requested,
     * so append the optionName before the retrieved value.
     */
    Tcl_DStringAppendElement (dsPtr, "-length");
    LTOA (chan->length, buffer);
    Tcl_DStringAppendElement (dsPtr, buffer);

    Tcl_DStringAppendElement (dsPtr, "-allocated");
    LTOA (chan->length, buffer);
    Tcl_DStringAppendElement (dsPtr, buffer);

  } else if (0 == strcmp (optionName, "-length")) {
    LTOA (chan->length, buffer);
    Tcl_DStringAppendElement (dsPtr, buffer);
  } else if (0 == strcmp (optionName, "-allocated")) {
    LTOA (chan->length, buffer);
    Tcl_DStringAppendElement (dsPtr, buffer);
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	WatchChannel --
 *
 *	------------------------------------------------*
 *	Initialize the notifier to watch Tcl_Files from
 *	this channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Sets up the notifier so that a future
 *		event on the channel will be seen by Tcl.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */
	/* ARGSUSED */
static void
WatchChannel (instanceData, mask)
ClientData instanceData;	/* Channel to watch */
int        mask;		/* Events of interest */
{
  /*
   * In-memory fifo channels are not based on files.
   * They are always writable, and almost always readable.
   * We could call Tcl_NotifyChannel immediately, but this
   * would starve other sources, so a timer is set up instead.
   */

  ChannelInstance* chan = (ChannelInstance*) instanceData;

  if (mask) {
    if (chan->timer == (Tcl_TimerToken) NULL) {
      chan->timer = Tcl_CreateTimerHandler (DELAY, ChannelReady, instanceData);
    }
  } else {
    if (chan->timer != (Tcl_TimerToken) NULL) {
      Tcl_DeleteTimerHandler (chan->timer);
    }
    chan->timer = (Tcl_TimerToken) NULL;
  }

  chan->interest = mask;
}

/*
 *------------------------------------------------------*
 *
 *	ChannelReady --
 *
 *	------------------------------------------------*
 *	Called by the notifier (-> timer) to check whether
 *	the channel is readable or writable.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of 'Tcl_NotifyChannel'.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ChannelReady (instanceData)
ClientData instanceData; /* Channel to query */
{
  /*
   * In-memory fifo channels are always writable (fileevent
   * writable) and they are readable if they are not empty.
   */

  ChannelInstance* chan = (ChannelInstance*) instanceData;
  int              mask = TCL_READABLE | TCL_WRITABLE;

  /*
   * Timer fired, our token is useless now.
   */

  chan->timer = (Tcl_TimerToken) NULL;

  if (!chan->interest) {
    return;
  }

  if (! FIFO_EMPTY (chan)) {
    mask &= ~TCL_READABLE;
  }

  /* Tell Tcl about the possible events.
   * This will regenerate the timer too, via 'WatchChannel'.
   */

  mask &= chan->interest;
  if (mask) {
    Tcl_NotifyChannel (chan->chan, mask);
  } else {
    chan->timer = Tcl_CreateTimerHandler (DELAY, ChannelReady, instanceData);
  }
}

/*
 *------------------------------------------------------*
 *
 *	GetFile --
 *
 *	------------------------------------------------*
 *	Called from Tcl_GetChannelHandle to retrieve
 *	OS handles from inside a in-memory fifo channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		The appropriate OS handle or NULL if not
 *		present. 
 *
 *------------------------------------------------------*
 */
static int
GetFile (instanceData, direction, handlePtr)
ClientData  instanceData;	/* Channel to query */
int         direction;		/* Direction of interest */
ClientData* handlePtr;          /* Space to the handle into */
{
  /*
   * In-memory fifo channels are not based on files.
   */

  /* *handlePtr = (ClientData) NULL; */
  return TCL_ERROR;
}

/*
 * ----------------------------------------------------------------------
 *
 * Memchan_CreateFifoChannel -
 *
 *	Create a memchan 'fifo' channel.
 *
 * Results:
 *	Returns the newly minted channel
 *
 * Side effects:
 *	A fifo channel is registered in the current interp.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Channel
Memchan_CreateFifoChannel(interp)
     Tcl_Interp *interp;	/* the current interp */
{
  Tcl_Obj*         channelHandle;
  Tcl_Channel      chan;
  ChannelInstance* instance;

  instance = (ChannelInstance*) Tcl_Alloc (sizeof (ChannelInstance));
  instance->length = 0;
  instance->queue  = Buf_NewQueue ();

  channelHandle = MemchanGenHandle ("fifo");

  chan = Tcl_CreateChannel (&channelType,
			    Tcl_GetStringFromObj (channelHandle, NULL),
			    (ClientData) instance,
			    TCL_READABLE | TCL_WRITABLE);

  instance->chan      = chan;
  instance->timer     = (Tcl_TimerToken) NULL;
  instance->interest  = 0;

  Tcl_RegisterChannel  (interp, chan);
  Tcl_SetChannelOption (interp, chan, "-buffering", "none");
  Tcl_SetChannelOption (interp, chan, "-blocking",  "0");

  return chan;
}

/*
 *------------------------------------------------------*
 *
 *	MemchanFifoCmd --
 *
 *	------------------------------------------------*
 *	This procedure realizes the 'fifo' command.
 *	See the manpages for details on what it does.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See the user documentation.
 *
 *	Result:
 *		A standard Tcl result.
 *
 *------------------------------------------------------*
 */
	/* ARGSUSED */
int
MemchanFifoCmd (notUsed, interp, objc, objv)
ClientData    notUsed;		/* Not used. */
Tcl_Interp*   interp;		/* Current interpreter. */
int           objc;		/* Number of arguments. */
Tcl_Obj*CONST objv[];		/* Argument objects. */
{
    Tcl_Channel chan;
    
    if (objc != 1) {
	Tcl_AppendResult (interp, "wrong # args: should be \"fifo\"",
	    (char*) NULL);
	return TCL_ERROR;
    }
    
    chan = Memchan_CreateFifoChannel(interp);
    Tcl_AppendResult(interp, Tcl_GetChannelName(chan), (char *)NULL);
    return TCL_OK;
}

