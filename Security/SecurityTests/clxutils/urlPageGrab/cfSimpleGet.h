/* 
 * cgGet.h - simplified interface to use CFNetwork to do one
 * HTTP or HTTPS GET transaction.
 */

#ifndef	_CF_SIMPLE_GET_H_
#define _CF_SIMPLE_GET_H_

#include <CoreFoundation/CoreFoundation.h>

CFDataRef cfSimpleGet(
	const char 	*url);

#endif	/* _CF_SIMPLE_GET_H_ */
