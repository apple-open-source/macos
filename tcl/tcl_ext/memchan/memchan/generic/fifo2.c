/*
 * fifo2.c --
 *
 *	Implementation of a bi-directional in-memory fifo.
 *
 * Copyright (C) 2000 Andreas Kupries (a.kupries@westend.com)
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
 * CVS: $Id: fifo2.c,v 1.8 2004/11/10 00:07:01 patthoyts Exp $
 */


#include "memchanInt.h"
#include "buf.h"

#if ((TCL_THREADS) && (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION == 2))

#error "The fifo2 is not supported with threaded tcl 8.2."

/*
 *  Tcl 8.2 supports Tcl_Mutexes but doesn't include the Tcl_MutexFinalize
 *  command. This means that we will leak a mutex on every close of a fifo2
 *  channel pair in this version of tcl -- ONLY THREADED TCL.
 *
 *  If you _really_ want to build memchan on threaded tcl 8.2 then comment
 *  out the #error line above and you can get these two stub functions
 *  which avoids the issue while permitting the remainder of memchan 
 *  to be built.
 */

void
Memchan_CreateFifo2Channel(interp, aPtr, bPtr)
     Tcl_Interp *interp;	/* the current interp */
     Tcl_Channel *aPtr;		/* pointer to channel A */
     Tcl_Channel *bPtr;		/* pointer to channel B */
{
    Tcl_SetResult(interp, 
	"fifo2 is not supported in threaded tcl 8.2", TCL_STATIC);
    return;
}

int
MemchanFifo2Cmd (notUsed, interp, objc, objv)
     ClientData    notUsed;	/* Not used. */
     Tcl_Interp*   interp;	/* Current interpreter. */
     int           objc;	/* Number of arguments. */
     Tcl_Obj*CONST objv[];	/* Argument objects. */
{
    Tcl_SetResult(interp, 
	"fifo2 is not supported in threaded tcl 8.2", TCL_STATIC);
    return TCL_ERROR;
}

#else


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
  "memory/fifo2",	/* Type name.                                    */
  (Tcl_ChannelTypeVersion)BlockMode, /* Set blocking  behaviour.         */
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
 * General structure and behaviour of a bi-directional fifo with 2
 * channels as its access-ports:
 *
 * ----------    --------                --------    ----------
 * |Channel |<---|Fifo  |--------------->|Fifo  |--->|Channel |
 * |        |--->|Inst. |   ----------   |Inst. |<---|        |
 * |        |    |      |-->| Queue> |<--|      |    |        |
 * ----------    | I    |   ----------   | II   |    ----------
 *               |      |   ----------   |      |
 *               |      |-->| <Queue |<--|      |
 *               |      |   ----------   |      |
 *               |      |<---------------|      |
 *               |      |     ------     |      |
 *               |      |---->|Mutx|<----|      |
 *               --------     ------     --------
 *
 * Communication between the two instances: I would have liked to use
 * the event queues of the core (and the Tcl_ThreadQueueEvent,
 * Tcl_ThreadAlert combo), but I cannot. To use them I have to know
 * the id of the thread a channel resides in. This information is
 * neither available nor are channels able to track their
 * location. The alternative is to directly access both structures and
 * to use mutexes for protection. Another alternative would be to use
 * an additional set of queues for signaling.
 *
 *
 * -----------------------------------------------------------------
 * NOTE -- TIP #10 was accepted for Tcl 8.4. This means that the above
 * information is available for this version of Tcl, and beyond. Now
 * use it.
 * -----------------------------------------------------------------
 *
 *
 * Whenever one of the channels has to do something it uses the mutex
 * to enforce exclusion of the other side. This enables it to
 * manipulate both sides without fear of inconsistency.
 * 
 * To enable the true generation of fileevents both instances keep
 * track of the state of queues (empty, not empty flagging) and notify
 * each other. Wether the notification will then cause the execution
 * of fileevents then depends on the interest set by the user of the
 * channels. Timers are set to check the flags periodically.
 *
 * Rules (View of Instance I).
 *
 *       Write to Queue>. If the queue was empty before send a
 *       'readable' event.
 *
 *	If receiving a 'readable' event check the size of the queue
 *	<Queue. If not empty and there is interest propagate the event
 *	via Tcl_NotifyChannel.
 *
 * Rules (View of Instance II).
 *
 *       Write to <Queue. If the queue was empty before send a
 *       'readable' event.
 *
 *	If receiving a 'readable' event check the size of the queue
 *	Queue>. If not empty and there is interest propagate the event
 *	via Tcl_NotifyChannel.
 *
 * [xx]
 * If a channel closes it marks its own instance structure as dead and
 * notifies the other side. It does not remove its instance
 * structure. This is done only after the other side closes
 * too. Because of the global mutex the channel cannot find a dead
 * structure on the other side it doesn't know about before. A channel
 * already knowing that the other side is dead will do no
 * notifications but proceed immediately to the stage of destroying
 * all structures associated to the fifos.  */

typedef struct ChannelLock {
#if (GT81)
  Tcl_Mutex lock; /* Global lock for both structures */
#else
  long dummy;
#endif
} ChannelLock;

/*
 * This structure describes the per-instance state of an in-memory
 * fifo channel. Two such structures make up one bi-directional fifo.
 */

typedef struct ChannelInstance {
  Tcl_Channel             chan;     /* Backreference to generic channel
				     * information */
  struct ChannelInstance* otherPtr; /* Reference to the other side of the
				     * bi-directional fifo. */
  int                     dead;     /* 0 - Channel is active.
				     * 1 - Channel is dead and event in
				     *     flight to the other side.
				     * 2 - Channel is dead two times,
				     *     an event from the other side
				     *     was here earlier. */
  int                     eof;      /* 0 - Other side is active
				     * 1 - Other side is dead. */
  Tcl_TimerToken          timer;    /* Timer used to link the channel into
				     * the notifier. */
  int                   interest;   /* Interest in events as signaled by the
				     * user of the channel */
  Buf_BufferQueue         wQueue;   /* Queue of buffers. Holds the information
				     * written to the other side. Thread safe
				     * by itself. "Queue >" in the diagram
				     * above. */
  Buf_BufferQueue         rQueue;   /* Queue of buffers. Holds the information
				     * written by the other side. Thread safe
				     * by itself. "Queue <" in the diagram
				     * above. */
  ChannelLock*            lock;     /* Global lock used by both sides to
				     * exclude the other. */
} ChannelInstance;

/* Definitions for DEADness and event types.
 */

#define FIFO_ALIVE (0) /* Channel is not dead */
#define FIFO_DEAD  (1) /* Channel is dead */

/* Macros to simplify the code below (no #ifdef's for the locking
 * functionality)
 */

#if (GT81)
#define MLOCK(chan) Tcl_MutexLock     (&((chan)->lock->lock))
#define MUNLK(chan) Tcl_MutexUnlock   (&((chan)->lock->lock))
#if (TCL_MAJOR_VERSION >= 8) && (TCL_MINOR_VERSION == 2)
#  define MFIN(chan)
#else
#  define MFIN(chan)  Tcl_MutexFinalize (&((chan)->lock->lock))
#endif
#else
#define MLOCK(chan) 
#define MUNLK(chan) 
#define MFIN(chan)  
#endif

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
ClientData  instanceData;    /* The instance information
			      * of the channel to close */
Tcl_Interp* interp;          /* unused */
{
  /* See [xx] fro a description of what is happening here.
   */

  ChannelInstance* chan = (ChannelInstance*) instanceData;

  /*
   * Get the lock and look at the state of both sides.  If the other
   * side is not dead just mark us as dead and return after releasing
   * the lock. If the other side was dead we are responsible for
   * cleaning up both sides. As we know the other side cannot anything
   * anymore we can release the lock earlier without fear. This is
   * even necessary to allow is to remove it altogether.
   *
   * A timer polling the instance has to be removed immediately.
   */

  MLOCK (chan);

  if (chan->timer != (Tcl_TimerToken) NULL) {
    Tcl_DeleteTimerHandler (chan->timer);
    chan->timer = (Tcl_TimerToken) NULL;
  }

  if (!chan->eof) {
    /*
     * Other side still living, mark us as dead on both sides, then
     * return without doing anything. It is responsibility of the
     * surviving side to clean up after us.
     */

    chan->dead          = FIFO_DEAD;
    chan->otherPtr->eof = 1;
    MUNLK (chan);
    return 0;
  }

  /*
   * The other side is alread dead. Proceed to remove all the
   * associated structures on both sides. Begin with the lock and the
   * queues.
   *
   * Unlocking the mutex is no problem, as the other is dead, i.e. not
   * doing anything anymore.
   */

  MUNLK (chan);
  MFIN  (chan);

  chan->otherPtr->lock = (ChannelLock*) NULL;
  Tcl_Free ((char*) chan->lock);
  chan->lock           = (ChannelLock*) NULL;

  chan->otherPtr->rQueue = (Buf_BufferQueue) NULL;
  chan->otherPtr->wQueue = (Buf_BufferQueue) NULL;

  Buf_FreeQueue (chan->rQueue);
  Buf_FreeQueue (chan->wQueue);

  chan->rQueue = (Buf_BufferQueue) NULL;
  chan->wQueue = (Buf_BufferQueue) NULL;

  chan->otherPtr->otherPtr = (ChannelInstance*) NULL;
  Tcl_Free ((char*) chan->otherPtr);

  chan->otherPtr = (ChannelInstance*) NULL;
  Tcl_Free ((char*) chan);

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
  ChannelInstance* chan = (ChannelInstance*) instanceData;
  int result = 0;

  MLOCK (chan);

  if (chan->dead) {
    /* single exception to 'goto done'. Required because the
     * mutex has to be unlocked before signaling the error.
     */

    MUNLK (chan);
    Tcl_Panic ("Trying to read from a dead channel");
    return 0;
  }

  if (toRead == 0) {
    result = 0;
    goto done;
  }

  /* The queue is thread-safe, no problem in simply
   * accessing it for the read.
   */

  toRead        = Buf_QueueRead (chan->rQueue, buf, toRead);
  *errorCodePtr = 0;

  if (toRead == 0) {
    /* No data available. If no eof came from the other side it
     * is only temporarily, so we signal this to the caller via
     * EWOULDBLOCK.
     */

    if (!chan->eof) {
      *errorCodePtr = EWOULDBLOCK;
      result        = -1;
      goto done;
    }
  }

  result = toRead;

done:
  MUNLK (chan);
  return result;
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
  ChannelInstance* chan = (ChannelInstance*) instanceData;
  int result;

  MLOCK (chan);

  if (chan->dead) {
    /* Single exception to 'goto done'. Required because the
     * mutex has to be unlocked before signaling the error.
     */

    MUNLK (chan);
    Tcl_Panic ("Trying to write to a dead channel");
    return 0;
  }

  if (toWrite == 0) {
    result = 0;
    goto done;
  }

  /* The queue is thread-safe, no problem in simply
   * accessing it for the write.
   */

  /*
   * If the other side is dead there is no point in actually writing
   * to it anymore. But pretend that we did it anyway.
   */

  if (!chan->eof) {
    toWrite = Buf_QueueWrite (chan->wQueue, buf, toWrite);
  }

  result = toWrite;

done:
  MUNLK (chan);
  return result;
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
   * read-only, fconfigure options.
   *
   * "rlength" obtains the number of bytes currently stored
   *           and waiting to be read by us.
   * "wlength" obtains the number of bytes currently stored
   *           and waiting to be read by the other side.
   */

  ChannelInstance* chan = (ChannelInstance*) instanceData;
  char             buffer [50];
  /* sufficient even for 64-bit quantities */

  MLOCK (chan);

  if (chan->dead) {
    /* Single exception to 'goto done'. Required because the
     * mutex has to be unlocked before signaling the error.
     */

    MUNLK (chan);
    Tcl_Panic ("Trying to get options from a dead channel");
    return TCL_ERROR;
  }

  /* Known options:
   * -rlength: Number of bytes waiting for consumation by us.
   * -wlength: Number of bytes waiting for consumation by the other side.
   */

  if ((optionName != (char*) NULL) &&
      (0 != strcmp (optionName, "-rlength")) &&
      (0 != strcmp (optionName, "-wlength"))) {
    Tcl_SetErrno (EINVAL);

    MUNLK (chan);
    return Tcl_BadChannelOption (interp, optionName, "rlength wlength");
  }

  if (optionName == (char*) NULL) {
    /*
     * optionName == NULL
     * => a list of options and their values was requested,
     * so append the optionName before the retrieved value.
     */

    Tcl_DStringAppendElement (dsPtr, "-rlength");
    LTOA (Buf_QueueSize (chan->rQueue), buffer);
    Tcl_DStringAppendElement (dsPtr, buffer);
    
    Tcl_DStringAppendElement (dsPtr, "-wlength");
    LTOA (Buf_QueueSize (chan->wQueue), buffer);
    Tcl_DStringAppendElement (dsPtr, buffer);

  } else if (0 == strcmp (optionName, "-rlength")) {
    LTOA (Buf_QueueSize (chan->rQueue), buffer);
    Tcl_DStringAppendElement (dsPtr, buffer);
  } else if (0 == strcmp (optionName, "-wlength")) {
    
    Tcl_DStringAppendElement (dsPtr, "-wlength");
    LTOA (Buf_QueueSize (chan->wQueue), buffer);
    Tcl_DStringAppendElement (dsPtr, buffer);
  }

  MUNLK (chan);
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
   * They are almost always writable, and almost always readable.
   * We could call Tcl_NotifyChannel immediately, but this
   * would starve other sources, so a timer is set up instead.
   */

  ChannelInstance* chan = (ChannelInstance*) instanceData;

  MLOCK (chan);

  if (chan->dead) {
    MUNLK (chan);
    Tcl_Panic ("Trying to watch a dead channel");
    return;
  }

  /* Check the interest of the caller against the state of the
   * channel. There is no need to start a timer if no events
   * are possible.
   */

  if (chan->eof) {
    /* The other side is dead. This means that our side will
     * never be writable. And if our read queue is empty we
     * are unreadable too. There is no point in trying to
     * generate events nevertheless.
     */

    mask &= ~TCL_WRITABLE;
    /**if (Buf_QueueSize (chan->rQueue) == 0) {
      mask &= ~TCL_READABLE;
    }**/
  }

  if (mask) {
    if (chan->timer == (Tcl_TimerToken) NULL) {
      chan->timer = Tcl_CreateTimerHandler (DELAY, ChannelReady, instanceData);
    }
  } else {
    if (chan->timer != (Tcl_TimerToken) NULL) {
      Tcl_DeleteTimerHandler (chan->timer);
      chan->timer = (Tcl_TimerToken) NULL;
    }
  }

  chan->interest = mask;
  MUNLK (chan);
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
   * In-memory fifo channels are almost always writable (fileevent
   * writable) and they are readable if they are not empty.
   */

  ChannelInstance* chan = (ChannelInstance*) instanceData;
  int              events;

  MLOCK (chan);

  /*
   * Compute two interest masks from the current state: The first
   * tells us, wether wee have to generate some events, the second,
   * wether we should recreate the timer. The second is required only
   * if there were no events this time.
   */

  chan->timer = (Tcl_TimerToken) NULL;

  events = 0;
  if ((Buf_QueueSize (chan->rQueue) > 0) || chan->eof) {
    events |= TCL_READABLE;
  }
  if (!chan->eof) {
    events |= TCL_WRITABLE;
  }

  events &= chan->interest;

  if (events) {
    /*
     * We have to release the lock before notifying the channel of new
     * events for two reasons:
     *
     * 1. We must not lock out the other side for the indefinite time
     *    required to handle the events.
     *
     * 2. The handler is allowed to close the channel and not all
     *    platforms allow us to lock a mutex twice. So not releasing
     *    it here may cause us to lock ourselves out.
     */

    MUNLK (chan);
    Tcl_NotifyChannel (chan->chan, events);
    return;
  }

  /* Check this more exactly, I think we can scrap that and assert
   * (future == 0)
   */

  /*
   * There were no events this time. Check wether we can generate
   * events in the future. If yes, we recreate the timer by ourselves.
   */

  events = TCL_READABLE | TCL_WRITABLE;

  if (chan->eof) {
    /* The other side is dead. This means that our side will
     * never be writable. And if our read queue is empty we
     * are unreadable too. There is no point in trying to
     * generate events nevertheless.
     */

    events &= ~TCL_WRITABLE;
    if (Buf_QueueSize (chan->rQueue) == 0) {
      events &= ~TCL_READABLE;
    }
  }

  if (events) {
    chan->timer = Tcl_CreateTimerHandler (DELAY, ChannelReady, instanceData);
  }

  MUNLK (chan);
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
 * Memchan_CreateFifo2Channel -
 *
 *	Create pair of memchan 'fifo2' channels.
 *
 * Results:
 *	Sets pointers to the new channel instances.
 *
 * Side effects:
 *	Two linked channels are registered in the current interp.
 *
 * ----------------------------------------------------------------------
 */

void
Memchan_CreateFifo2Channel(interp, aPtr, bPtr)
     Tcl_Interp *interp;	/* the current interp */
     Tcl_Channel *aPtr;		/* pointer to channel A */
     Tcl_Channel *bPtr;		/* pointer to channel B */
{
    Tcl_Obj*         channel [2];
    
    Tcl_Channel      chanA;
    Tcl_Channel      chanB;
    
    ChannelInstance* instanceA;
    ChannelInstance* instanceB;
    
    instanceA = (ChannelInstance*) Tcl_Alloc (sizeof (ChannelInstance));
    instanceA->rQueue   = Buf_NewQueue ();
    instanceA->wQueue   = Buf_NewQueue ();
    instanceA->timer    = (Tcl_TimerToken) NULL;
    instanceA->dead     = FIFO_ALIVE;
    instanceA->eof      = 0;
    instanceA->interest = 0;
    instanceA->lock     = (ChannelLock*) Tcl_Alloc (sizeof (ChannelLock));
    
    instanceB = (ChannelInstance*) Tcl_Alloc (sizeof (ChannelInstance));
    instanceB->rQueue   = instanceA->wQueue;
    instanceB->wQueue   = instanceA->rQueue;
    instanceB->timer    = (Tcl_TimerToken) NULL;
    instanceB->dead     = FIFO_ALIVE;
    instanceB->eof      = 0;
    instanceB->interest = 0;
    instanceB->lock     = instanceA->lock;
    
    /* bug #996078 - Tcl_Mutex expects the mutex to be NULL */
    memset((ChannelLock *)instanceA->lock, 0, sizeof (ChannelLock));
    
    instanceA->otherPtr = instanceB;
    instanceB->otherPtr = instanceA;
    
    channel [0]        = MemchanGenHandle ("fifo");
    channel [1]        = MemchanGenHandle ("fifo");
    
    chanA = Tcl_CreateChannel (&channelType,
			       Tcl_GetStringFromObj (channel [0], NULL),
			       (ClientData) instanceA,
			       TCL_READABLE | TCL_WRITABLE);
    
    instanceA->chan      = chanA;
    
    chanB = Tcl_CreateChannel (&channelType,
			       Tcl_GetStringFromObj (channel [1], NULL),
			       (ClientData) instanceB,
			       TCL_READABLE | TCL_WRITABLE);
    
    instanceB->chan      = chanB;
    
    
    Tcl_RegisterChannel  (interp, chanA);
    Tcl_SetChannelOption (interp, chanA, "-buffering", "none");
    Tcl_SetChannelOption (interp, chanA, "-blocking",  "0");
    
    Tcl_RegisterChannel  (interp, chanB);
    Tcl_SetChannelOption (interp, chanB, "-buffering", "none");
    Tcl_SetChannelOption (interp, chanB, "-blocking",  "0");

    *aPtr = chanA;
    *bPtr = chanB;
}

/*
 *------------------------------------------------------*
 *
 *	MemchanFifo2Cmd --
 *
 *	------------------------------------------------*
 *	This procedure realizes the 'fifo2' command.
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
MemchanFifo2Cmd (notUsed, interp, objc, objv)
     ClientData    notUsed;	/* Not used. */
     Tcl_Interp*   interp;	/* Current interpreter. */
     int           objc;	/* Number of arguments. */
     Tcl_Obj*CONST objv[];	/* Argument objects. */
{
    Tcl_Obj    *channel[2];
    Tcl_Channel chanA;
    Tcl_Channel chanB;
    
    if (objc != 1) {
	Tcl_AppendResult (interp,
			  "wrong # args: should be \"fifo2\"",
			  (char*) NULL);
	return TCL_ERROR;
    }
    
    /* 
     * We create two instances, connect them together and
     * return a list containing both names.
     */
    
    Memchan_CreateFifo2Channel(interp, &chanA, &chanB);
    channel[0] = Tcl_NewStringObj(Tcl_GetChannelName(chanA), -1);
    channel[1] = Tcl_NewStringObj(Tcl_GetChannelName(chanB), -1);
    Tcl_SetObjResult (interp, Tcl_NewListObj(2, channel));
    return TCL_OK;
}

#endif /* !threaded tcl 8.2 */
