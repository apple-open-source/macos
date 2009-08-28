/*
 * Copyright (c) 2008 Apple Inc.  All Rights Reserved.
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
 * 
 * Does this need to be released under the OpenLDAP License instead of, or in
 * addition to the APSL?
 * 
 */

/*
 * Header file for red black tree of the ldap response messages.
 */
 

#ifndef _RB_RESPONSE_H_
#define _RB_RESPONSE_H_
 
#ifdef LDAP_RESPONSE_RB_TREE

#include "ldap.h"
#include "ldap-int.h"
#include "lber_types.h"
 
void ldap_resp_rbt_create( LDAP *ld );
void ldap_resp_rbt_free( LDAP *ld );
void ldap_resp_rbt_insert_msg( LDAP *ld, LDAPMessage *lm );
void ldap_resp_rbt_delete_msg( LDAP *ld, LDAPMessage *lm );
void ldap_resp_rbt_unlink_msg( LDAP *ld, LDAPMessage *lm);
void ldap_resp_rbt_unlink_partial_msg( LDAP *ld, LDAPMessage *lm );
LDAPMessage *ldap_resp_rbt_find_msg( LDAP* ld, ber_int_t msgid );
LDAPMessage *ldap_resp_rbt_get_first_msg( LDAP *ld );
LDAPMessage *ldap_resp_rbt_get_next_msg( LDAP *ld, LDAPMessage *lm );
void ldap_resp_rbt_dump( LDAP *ld );

#endif /* LDAP_RESPONSE_RB_TREE */

#endif /* _RB_RESPONSE_H_ */
 