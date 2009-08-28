/*
 * random.c --
 *
 *	Implementation of a random Tcl file channel
 *
 *  The PRNG in use here is the ISAAC PRNG. See
 *    http://www.burtleburtle.net/bob/rand/isaacafa.html
 *  for details. This generator _is_ suitable for cryptographic use
 *
 * Copyright (C) 2004 Pat Thoyts <patthoyts@users.sourceforge.net>
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
 * CVS: $Id: random.c,v 1.5 2004/11/10 00:07:01 patthoyts Exp $
 */


#include "memchanInt.h"
#include "../isaac/rand.h"
#include <time.h>
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
static void	ChannelReady _ANSI_ARGS_((ClientData instanceData));
static int      GetFile      _ANSI_ARGS_((ClientData instanceData,
					  int direction,
					  ClientData* handlePtr));

static int	BlockMode _ANSI_ARGS_((ClientData instanceData,
				       int mode));

static int	GetOption _ANSI_ARGS_((ClientData instanceData,
				       Tcl_Interp* interp,
				       CONST84 char *optionName,
				       Tcl_DString *dsPtr));

static int	SetOption _ANSI_ARGS_((ClientData instanceData,
				       Tcl_Interp* interp,
				       CONST char *optionName,
				       CONST char *newValue));
/*
 * This structure describes the channel type structure for random channels:
 * random channels are not seekable. They have no options.
 */

static Tcl_ChannelType channelType = {
    "random",			/* Type name.                                */
    (Tcl_ChannelTypeVersion)BlockMode, /* Set blocking behaviour.            */
    Close,			/* Close channel, clean instance data        */
    Input,			/* Handle read request                       */
    Output,			/* Handle write request                      */
    NULL,			/* Move location of access point.  NULL'able */
    SetOption,			/* Set options.                    NULL'able */
    GetOption,			/* Get options.                    NULL'able */
    WatchChannel,		/* Initialize notifier                       */
#if GT81
    GetFile,			/* Get OS handle from the channel.           */
    NULL			/* Close2Proc, not available, no partial close
				 * possible */
#else
    GetFile			/* Get OS handle from the channel.           */
#endif
};


/*
 * This structure describes the per-instance state of a in-memory random channel.
 */

typedef struct ChannelInstance {
    Tcl_Channel    chan;   /* Backreference to generic channel information */
    Tcl_TimerToken timer;  /* Timer used to link the channel into the
			    * notifier. */
    struct randctx state;  /* PRNG state */
    int            delay;  /* fileevent notification interval (in ms) */
} ChannelInstance;

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
 *	when an in-memory random channel is closed.
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
     ClientData  instanceData;	/* The instance information of the channel to
				 * close */
     Tcl_Interp* interp;	/* unused */
{
    ChannelInstance* chan;
    
    chan = (ChannelInstance*) instanceData;
    
    if (chan->timer != (Tcl_TimerToken) NULL) {
	Tcl_DeleteTimerHandler (chan->timer);
    }
    
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
 *	level to read input from an in-memory random channel.
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
     char*      buf;		/* Buffer to fill */
     int        toRead;		/* Requested number of bytes */
     int*       errorCodePtr;	/* Location of error flag */
{
    ChannelInstance *chan = (ChannelInstance *)instanceData;    
    size_t n = 0, i = sizeof(unsigned long);
    unsigned long rnd;

    for (n = 0; toRead - n > i; n += i) {
	rnd = rand(&chan->state);
	memcpy(&buf[n], (char *)&rnd, i);
    }
    if (toRead - n > 0) {
	rnd = rand(&chan->state);
	memcpy(&buf[n], (char *)&rnd, toRead-n);
	n += (toRead-n);
    }

    return n;
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
     int        toWrite;	/* Number of bytes to write. */
     int*       errorCodePtr;	/* Location of error flag. */
{
    ChannelInstance *chan = (ChannelInstance *)instanceData;    
    ub4 rnd, n = 0;
    ub4 *s = (ub4 *)buf;
    ub4 *p = chan->state.randrsl;

    while (n < RANDSIZ && n < (ub4)(toWrite/4)) {
	p[n] ^= s[n]; n++;
    }
    /* mix the state */
    rnd = rand(&chan->state);

    /* 
     * If we filled the state with data, there is no advantage to
     * adding in additional data. So lets save time.
     */
    return toWrite;
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
     * random channels are not based on files.
     * They are always writable, and always readable.
     * We could call Tcl_NotifyChannel immediately, but this
     * would starve other sources, so a timer is set up instead.
     */
    
    ChannelInstance* chan = (ChannelInstance*) instanceData;
    
    if (mask) {
	if (chan->timer == (Tcl_TimerToken) NULL) {
	    chan->timer = Tcl_CreateTimerHandler(chan->delay, ChannelReady, 
		instanceData);
	}
    } else {
	Tcl_DeleteTimerHandler (chan->timer);
	chan->timer = (Tcl_TimerToken) NULL;
    }
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
     ClientData instanceData;	/* Channel to query */
{
    /*
     * In-memory random channels are always writable (fileevent
     * writable) and they are also always readable.
     */
    
    ChannelInstance* chan = (ChannelInstance*) instanceData;
    int              mask = TCL_READABLE | TCL_WRITABLE;
    
    /*
     * Timer fired, our token is useless now.
     */
    
    chan->timer = (Tcl_TimerToken) NULL;
    
    /* Tell Tcl about the possible events.
     * This will regenerate the timer too, via 'WatchChannel'.
     */
    
    Tcl_NotifyChannel (chan->chan, mask);
}

/*
 *------------------------------------------------------*
 *
 *	GetFile --
 *
 *	------------------------------------------------*
 *	Called from Tcl_GetChannelHandle to retrieve
 *	OS handles from inside a in-memory random channel.
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
     int         direction;	/* Direction of interest */
     ClientData* handlePtr;	/* Space to the handle into */
{
    /*
     * In-memory random channels are not based on files.
     */
    
    /* *handlePtr = (ClientData) NULL; */
    return TCL_ERROR;
}

/*
 *------------------------------------------------------*
 *
 *	SetOption --
 *
 *	------------------------------------------------*
 *	Set a channel option
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Channel parameters may be modified.
 *
 *	Result:
 *		A standard Tcl result. The new value of the
 *		specified option is returned as the interpeter
 *		result
 *
 *------------------------------------------------------*
 */

static int
SetOption (instanceData, interp, optionName, newValue)
     ClientData   instanceData;	/* Channel to query */
     Tcl_Interp   *interp;	/* Interpreter to leave error messages in */
     CONST char *optionName;	/* Name of requested option */
     CONST char *newValue;	/* The new value */
{
    ChannelInstance *chan = (ChannelInstance*) instanceData;
    CONST char *options = "delay";
    int result = TCL_OK;

    if (!strcmp("-delay", optionName)) {
	int delay = DELAY;
	result = Tcl_GetInt(interp, (CONST84 char *)newValue, &delay);
	if (result == TCL_OK) {
	    chan->delay = delay;
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(delay));
	}
    } else {
	result = Tcl_BadChannelOption(interp, 
	    (CONST84 char *)optionName, (CONST84 char *)options);
    }
    return result;
}

/*
 *------------------------------------------------------*
 *
 *	GetOption --
 *
 *	------------------------------------------------*
 *	Computes an option value for a zero
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
     Tcl_Interp*  interp;	/* Interpreter to leave error messages in */
     CONST84 char* optionName;	/* Name of reuqested option */
     Tcl_DString* dsPtr;	/* String to place the result into */
{
    ChannelInstance *chan = (ChannelInstance*) instanceData;
    char             buffer [50];
    
    /* Known options:
     * -delay:    Number of milliseconds between readable fileevents.
     */
    
    if ((optionName != (char*) NULL) &&
	(0 != strcmp (optionName, "-delay"))) {
	Tcl_SetErrno (EINVAL);
	return Tcl_BadChannelOption (interp, optionName, "delay");
    }
    
    if (optionName == (char*) NULL) {
	/*
	 * optionName == NULL
	 * => a list of options and their values was requested,
	 * so append the optionName before the retrieved value.
	 */
	Tcl_DStringAppendElement (dsPtr, "-delay");
	LTOA (chan->delay, buffer);
	Tcl_DStringAppendElement (dsPtr, buffer);
	
    } else if (0 == strcmp (optionName, "-delay")) {
	LTOA (chan->delay, buffer);
	Tcl_DStringAppendElement (dsPtr, buffer);
    }
    
    return TCL_OK;
}

/*
 *------------------------------------------------------
 *
 * Memchan_CreateRandomChannel -
 *
 * 	Mint a new 'random' channel.
 *
 * Result:
 *	Returns the new channel.
 *
 *------------------------------------------------------
 */

Tcl_Channel
Memchan_CreateRandomChannel(interp)
     Tcl_Interp *interp;	/* current interpreter */
{
    Tcl_Channel      chan;
    Tcl_Obj         *channelHandle;
    ChannelInstance *instance;
    unsigned long seed;

    instance      = (ChannelInstance*) Tcl_Alloc (sizeof (ChannelInstance));
    channelHandle = MemchanGenHandle ("random");

    chan = Tcl_CreateChannel (&channelType,
	Tcl_GetStringFromObj (channelHandle, NULL),
	(ClientData) instance,
	TCL_READABLE | TCL_WRITABLE);

    instance->chan      = chan;
    instance->timer     = (Tcl_TimerToken) NULL;
    instance->delay     = DELAY;

    /*
     * Basic initialization of the PRNG state
     */
    seed = time(NULL) + ((long)Tcl_GetCurrentThread() << 12);
    memcpy(&instance->state.randrsl, &seed, sizeof(seed));
    randinit(&instance->state);
    
    Tcl_RegisterChannel  (interp, chan);
    Tcl_SetChannelOption (interp, chan, "-buffering", "none");
    Tcl_SetChannelOption (interp, chan, "-blocking",  "0");

    return chan;
}

/*
 *------------------------------------------------------*
 *
 *	MemchanRandomCmd --
 *
 *	------------------------------------------------*
 *	This procedure realizes the 'random' command.
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
MemchanRandomCmd (notUsed, interp, objc, objv)
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
    
    chan = Memchan_CreateRandomChannel(interp);
    Tcl_AppendResult(interp, Tcl_GetChannelName(chan), (char *)NULL);
    return TCL_OK;
}
