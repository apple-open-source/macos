/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */

/*
 * ldapFetch.h - fetch an entity via LDAP
 */
 
#ifndef	_LDAP_FETCH_H_
#define _LDAP_FETCH_H_

#include <Security/cssmtype.h>

typedef enum {
	LT_Crl = 1,
	LT_Cert
} LF_Type;

/* fetch via LDAP */
CSSM_RETURN ldapFetch(
	const CSSM_DATA 	&url,
	LF_Type				lfType,
	CSSM_DATA			&fetched);	// mallocd and RETURNED

/* fetch via HTTP */
CSSM_RETURN httpFetch(
	const CSSM_DATA 	&url,
	CSSM_DATA			&fetched);	// mallocd and RETURNED
	
/* Fetch from net, we figure out the schema */
CSSM_RETURN netFetch(
	const CSSM_DATA 	&url,
	LF_Type				lfType,
	CSSM_DATA			&fetched);	// mallocd and RETURNED

#endif	/* _LDAP_FETCH_H_ */