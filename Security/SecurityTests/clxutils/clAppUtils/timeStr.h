#ifndef	_TIME_STR_H_
#define _TIME_STR_H_

#include <time.h>
#include <Security/x509defs.h>

#define UTC_TIME_STRLEN				13
#define CSSM_TIME_STRLEN			14		/* no trailing 'Z' */
#define GENERALIZED_TIME_STRLEN		15

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Given a string containing either a UTC-style or "generalized time"
 * time string, convert to a struct tm (in GMT/UTC). Returns nonzero on
 * error. 
 */
int appTimeStringToTm(
	const char			*str,
	unsigned			len,
	struct tm			*tmp);

typedef enum {
	TIME_UTC,
	TIME_CSSM,
	TIME_GEN
} timeSpec;

/* Caller must CSSM_FREE() the resulting string */
char *appTimeAtNowPlus(int secFromNow,
	timeSpec timeSpec);

/*** for backwards compatibility ***/

/*
 * Malloc and return UTC (2-digit year) time string, for time with specified
 * offset from present. This uses the stdlib gmtime(), which is not thread safe.
 * Even though this function protects the call with a lock, the TP also uses 
 * gmtime. It also does the correct locking for its own calls to gmtime()Êbut 
 * the is no way to synchronize TP's calls to gmtime() with the calls to this 
 * one other than only using this one when no threads might be performing TP ops.
 */
char *utcAtNowPlus(int secFromNow);

/*
 * Same thing, generalized time (4-digit year).
 */
char *genTimeAtNowPlus(int secFromNow);

/*
 * Free the string obtained from the above. 
 */
void freeTimeString(char *timeStr);

char *x509TimeToCssmTimestring(
	const CSSM_X509_TIME 	*x509Time,
	unsigned				*rtnLen);		// for caller's convenience

#ifdef	__cplusplus
}
#endif

#endif	/* _TIME_STR_H_ */
