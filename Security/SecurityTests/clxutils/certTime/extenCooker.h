/*
 * extenCooker.h - module to cook up random (but reasonable)
 *                 versions of cert extensions
 */
 

#ifndef	_EXTEN_COOKER_H_
#define _EXTEN_COOKER_H_

#include <Security/cssmtype.h>
#include <Security/x509defs.h>
#include <Security/cssmapple.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Define one extension test.
 */

/* 
 * Cook up this extension with random, reasonable values. 
 * Incoming pointer refers to extension-specific C struct, mallocd
 * and zeroed by main test routine.
 */
typedef void (*extenCreateFcn)(void *arg);

/*
 * Compare two instances of this extension. Return number of 
 * compare errors.
 */
typedef unsigned (*extenCompareFcn)(
	const void *preEncode, 
	const void *postEncode);
	
/*
 * Free struct components mallocd in extenCreateFcn. Do not free
 * the outer struct.
 */
typedef void (*extenFreeFcn)(void *arg);

typedef struct {
	/* three extension-specific functions */
	extenCreateFcn		createFcn;
	extenCompareFcn		compareFcn;
	extenFreeFcn		freeFcn;
	
	/* size of C struct passed to all three functions */
	unsigned			extenSize;
	
	/* the OID for this extension */
	CSSM_OID			extenOid;
	
	/* description for error logging and blob writing */
	const char			*extenDescr;
	
	/* command-line letter for this one */
	char				extenLetter;
	
} ExtenTest;

/*
 * extenCooker.cpp - module to cook up random (but reasonable)
 *                   versions of cert extensions
 */

#include <stdio.h>
#include <stdlib.h>
#include "extenCooker.h"

CSSM_BOOL randBool();

/* Fill a CSSM_DATA with random data. Its referent is allocd with malloc. */
void randData(	
	CSSM_DATA_PTR	data,
	uint8			maxLen);


/*
 * Various compare tests
 */
int compBool(
	CSSM_BOOL pre,
	CSSM_BOOL post,
	char *desc);


int compCssmData(
	CSSM_DATA &d1,
	CSSM_DATA &d2,
	char *desc);
	

#pragma mark --- CE_KeyUsage ---
void kuCreate(void *arg);
unsigned kuCompare(const void *pre, const void *post);
/* no free */

#pragma mark --- CE_BasicConstraints ---
void bcCreate(void *arg);
unsigned bcCompare(const void *pre, const void *post);
/* no free */

#pragma mark --- CE_SubjectKeyID ---
void skidCreate(void *arg);
unsigned skidCompare(const void *pre, const void *post);
void skidFree(void *arg);

#pragma mark --- CE_NetscapeCertType ---
void nctCreate(void *arg);
unsigned nctCompare(const void *pre, const void *post);
/* no free */

#pragma mark --- CE_ExtendedKeyUsage ---
void ekuCreate(void *arg);
unsigned ekuCompare(const void *pre, const void *post);
/* no free */

#pragma mark --- general purpose X509 name generator ---
void rdnCreate(
	CSSM_X509_RDN_PTR rdn);
unsigned rdnCompare(
	CSSM_X509_RDN_PTR rdn1,
	CSSM_X509_RDN_PTR rdn2);
void rdnFree(
	CSSM_X509_RDN_PTR rdn);

void x509NameCreate(
	CSSM_X509_NAME_PTR x509Name);
unsigned x509NameCompare(
	const CSSM_X509_NAME_PTR n1,
	const CSSM_X509_NAME_PTR n2);
void x509NameFree(
	CSSM_X509_NAME_PTR n);

#pragma mark --- general purpose GeneralNames generator ---

void genNamesCreate(void *arg);
unsigned genNamesCompare(const void *pre, const void *post);
void genNamesFree(void *arg);

#pragma mark --- CE_CRLDistPointsSyntax ---
void cdpCreate(void *arg);
unsigned cdpCompare(const void *pre, const void *post);
void cdpFree(void *arg);

#pragma mark --- CE_AuthorityKeyID ---
void authKeyIdCreate(void *arg);
unsigned authKeyIdCompare(const void *pre, const void *post);
void authKeyIdFree(void *arg);

#pragma mark --- CE_CertPolicies ---
void cpCreate(void *arg);
unsigned cpCompare(const void *pre, const void *post);
void cpFree(void *arg);

#ifdef	__cplusplus
}
#endif

#endif	/* _EXTEN_COOKER_H_ */
