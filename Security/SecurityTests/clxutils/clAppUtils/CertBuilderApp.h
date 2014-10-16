/*
 * CertBuilderApp.cpp - support for constructing certs, CDSA version
 */

#ifndef	_CERT_BUILDER_APP_H_
#define _CERT_BUILDER_APP_H_

#include <Security/cssm.h>
#include <Security/x509defs.h>
#include <time.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Name/OID pair used in CB_BuildX509Name
 */
typedef struct {
	const char 			*string;
	const CSSM_OID 		*oid;
} CB_NameOid;

/*
 * Build up a CSSM_X509_NAME from an arbitrary list of name/OID pairs. 
 * We do one a/v pair per RDN.
 */
CSSM_X509_NAME *CB_BuildX509Name(
	const CB_NameOid 	*nameArray,
	unsigned 			numNames);

/* free the CSSM_X509_NAME obtained from CB_BuildX509Name */
void CB_FreeX509Name(
	CSSM_X509_NAME 		*top);

/* Obtain a CSSM_X509_TIME representing "now" plus specified seconds, or
* from a preformatted gen time string */
CSSM_X509_TIME *CB_BuildX509Time(
	unsigned secondsFromNow,		/* ignored if timeStr non-NULL */
	const char *timeStr=NULL);		/* optional, from genTimeAtNowPlus */
	
/* Free CSSM_X509_TIME obtained in CB_BuildX509Time */
void CB_FreeX509Time(
	CSSM_X509_TIME		*xtime);

CSSM_DATA_PTR CB_MakeCertTemplate(
	/* required */
	CSSM_CL_HANDLE			clHand,
	uint32					serialNumber,
	const CSSM_X509_NAME	*issuerName,	
	const CSSM_X509_NAME	*subjectName,
	const CSSM_X509_TIME	*notBefore,	
	const CSSM_X509_TIME	*notAfter,	
	const CSSM_KEY_PTR		subjectPubKey,
	CSSM_ALGORITHMS			sigAlg,			// e.g., CSSM_ALGID_SHA1WithRSA
	/* optional */
	const CSSM_DATA			*subjectUniqueId,
	const CSSM_DATA			*issuerUniqueId,
	CSSM_X509_EXTENSION		*extensions,
	unsigned				numExtensions);

#ifdef	__cplusplus
}
#endif
#endif /* _CERT_BUILDER_APP_H_ */
