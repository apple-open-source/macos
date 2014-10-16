#ifndef	_P12_PARSE_H_
#define _P12_PARSE_H_

#include <Security/cssmtype.h>
#include <CoreFoundation/CFString.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern int p12ParseTop(
	CSSM_DATA		&rawBlob,
	CSSM_CSP_HANDLE cspHand,
	CFStringRef 	pwd,
	bool 			verbose);

#ifdef	__cplusplus
}
#endif

#endif	/* _P12_PARSE_H_ */
