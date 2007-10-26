/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
#ifndef __LDAP_SUPPORT_FUNCTIONS__
#define __LDAP_SUPPORT_FUNCTIONS__
 
#include <ldap.h>
#include "CLDAPv3Plugin.h"

#define IsFatalLDAPError(a)		(a == LDAP_SERVER_DOWN || a == LDAP_UNAVAILABLE || a == LDAP_BUSY || a == LDAP_TIMEOUT || a == LDAP_CONNECT_ERROR)

__BEGIN_DECLS

tDirStatus
DSInitiateOrContinueSearch(
	sLDAPContextData   *inContext,
	sLDAPContinueData  *inContinue,
	char			   *inSearchBase,
	char			  **inAttrList,
	ber_int_t			inScope,
	char			   *inQueryFilter,
	LDAPMessage		  **outResult );

tDirStatus DSRetrieveSynchronous(
	char			   *inSearchBase,
	char			  **inAttrs,
	sLDAPContextData  *inContext,
	ber_int_t			inScope,
	char			   *inQueryFilter,
	LDAPMessage		  **outResult,
	char			  **outDN );

char *
GetDNForRecordName(
	char			   *inRecName,
	sLDAPContextData   *inContext,
	const char		   *inRecordType );

tDirStatus
ldapParseAuthAuthority(
	const char		   *inAuthAuthority,
	char			  **outVersion,
	char			  **outAuthTag,
	char			  **outAuthData );

void
VerifyKerberosForRealm(
	const char *inRealmName,
	const char *inServer );

__END_DECLS

#endif
