#ifndef	_TIME_STR_H_
#define _TIME_STR_H_

#include <time.h>

#define UTC_TIME_STRLEN				13
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

#ifdef	__cplusplus
}
#endif

#endif	/* _TIME_STR_H_ */