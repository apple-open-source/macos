/* Progress.h */

#ifndef _progress_h_
#define _progress_h_

#ifndef _xfer_h
#include "Xfer.h"
#endif

/* Progress-meter types. */
#define kPrNone 0
#define kPrPercent 1
#define kPrPhilBar 2
#define kPrKBytes 3
#define kPrDots 4
#define kPrStatBar 5
#define kPrLast kPrStatBar

/* Messages we pass to the current progress meter function. */
#define kPrInitMsg 1
#define kPrUpdateMsg 2
#define kPrEndMsg 3

/* This message is passed to ProgressReport() from EndProgress(). */
#define kPrLastUpdateMsg 4

/* This message is returned by a p.m. function (after receiving a
 * kPrEndMsg) if it wants EndProgress() to print the final transfer
 * statistics.
 */
#define kPrWantStatsMsg 5

/* This is how many seconds we wait between visual updates.  We don't
 * want to spend too much time tweaking the meter when we should be
 * doing the transferring!
 */
#define kDelaySeconds 2

/* Parameter to ProgressReport, specifying if we have to do an update,
 * or if we only should do it if needed.
 */
#define kOptionalUpdate 0
#define kForceUpdate 1

/* Note that we may start writing at the end of an existing file, so if
 * you want to know how much data is in the file you need to know how
 * much you skipped, and how much you actually wrote yourself.
 */
#define LOCALSIZE(a) ((a)->bytesTransferred + (a)->startPoint)

#define kKilobyte 1024
#define kMegabyte (kKilobyte * 1000)
#define kGigabyte ((long) kMegabyte * 1000L)


int StartProgress(XferSpecPtr);
void ProgressReport(XferSpecPtr, int);
void EndProgress(XferSpecPtr);
void TimeValSubtract(struct timeval *, struct timeval *, struct timeval *);
double TransferRate(long, struct timeval *, struct timeval *, char **, double *);
int PrNone(XferSpecPtr, int),
	PrPercent(XferSpecPtr, int),
	PrPhilBar(XferSpecPtr, int),
	PrKBytes(XferSpecPtr, int),
	PrDots(XferSpecPtr, int),
	PrStatBar(XferSpecPtr, int);

#endif /* _progress_h_ */

/* eof progress.h */
