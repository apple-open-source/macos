/* 
 * parseCert.h - text-based cert parser using CL
 */

#ifndef	_PARSE_CERT_H_
#define _PARSE_CERT_H_

#include <Security/cssmtype.h>
#include "oidParser.h"

#ifdef	__cplusplus
extern "C" {
#endif

/* print one field */
void printCertField(
	const CSSM_FIELD 		&field,
	OidParser 				&parser,
	CSSM_BOOL				verbose);

int printCert(
	const  unsigned char 	*certData,
	unsigned				certLen,
	CSSM_BOOL				verbose);

void printCertShutdown();

#ifdef	__cplusplus
}
#endif

#endif	/* _PARSE_CERT_H_ */
