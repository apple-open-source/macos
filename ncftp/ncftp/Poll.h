/* Poll.h */

/* We use poll/select to check for i/o in increments.
 * This first one, kTimeOutLen, is how long each one of these increments
 * is in seconds.  If we wait this long and i/o isn't ready yet, we
 * take the opportunity to update the progress meters.
 */
#define kTimeOutLen 2

/* If we got kMaxConsecTimeOuts of these increments in a row, we conclude
 * that the host isn't responding anymore and abort the transfer.
 */
#define kMaxConsecTimeOuts (gNetworkTimeout / kTimeOutLen)

#if !defined (XFER_USE_SELECT) && !defined (XFER_USE_POLL)
#	if defined(HAVE_SELECT) && !defined(HAVE_POLL)
#		define XFER_USE_SELECT 1
#	endif
#	if !defined(HAVE_SELECT) && defined(HAVE_POLL)
#		define XFER_USE_POLL 1
#	endif
#	if defined(HAVE_SELECT) && defined(HAVE_POLL)
	/* Supposedly poll() is faster on System V.  But poll() didn't work
	 * with non-STREAMS descriptors until System V.4.
	 */
#		if defined(SVR4) || defined(__svr4__)
#			define XFER_USE_POLL 1
#		else
#			define XFER_USE_SELECT 1
#		endif
#	endif
#	if !defined(HAVE_SELECT) && !defined(HAVE_POLL)
		/* Didn't detect select() or poll().
		 * You must have at least one of them, so hope they really have
		 * select(), which is the older and more supported one.
		 */
#		define XFER_USE_SELECT 1
#	endif
#endif

/* You can define this symbol to add gobs of debugging stuff to the trace.
 * This works for both the select and poll portions.
 */
#ifndef POLL_LOG
#	define POLL_LOG 0
#endif

#ifndef _xfer_h_
#	include "Xfer.h"
#endif

void InitPoll(XferSpecPtr);
int PollRead(XferSpecPtr);
int PollWrite(XferSpecPtr);
