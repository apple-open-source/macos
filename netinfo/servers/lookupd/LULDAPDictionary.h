/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * LULDAPDictionary.h
 * Abstraction of LDAP entry
 * Copyright (C) 1997 Luke Howard. All rights reserved.
 * Luke Howard, March 1997.
 */

#import "LUAgent.h"
#import "LUDictionary.h"
#import "LDAPAgent.h"

@interface LULDAPDictionary : LUDictionary
{
	LDAPMessage *entry;
	char *dn;
}

/*
 * designated initializer
 */
- (id)initWithEntry:(LDAPMessage *)entry agent:(id)agent;

/*
 * binds the value of the relative distinguished name to the
 * dictionary's "name" key. eg. an entry with a DN of
 * cn=Luke, o=Apple, c=US would have the value "Luke"
 * for the key "name" after binding.
 */
- (BOOL)bindRdnToName:(oid_name_t)attr;

/*
 * returns the values for an LDAP attribute. Must be
 * freed by caller with ldap_value_free()
 */
- (char **)entryValuesForAttribute:(oid_name_t)attrname;

/*
 * useful for objects with no fixed schema (where
 * you'll typically want to avoid interpolating
 * the "cn" or other naming attribute
 */
- (BOOL)bindAllAttributesExcept:(oid_name_t)attrname;

/*
 * binds all values of an LDAP attribute attrname
 * to the key specified by keyname
 */
- (BOOL)bindAttribute:(oid_name_t)attrname
			toKey:(char *)keyname;

/*
 * as above, excepts value specified in argument list
 */
- (BOOL)bindAttribute:(oid_name_t)attrname
			toKey:(char *)keyname
			exceptValue:(char *)value;

/*
 * skip the {crypt} prefix
 */
- (BOOL)bindAttributeCrypted:(oid_name_t)attrname
			toKey:(char *)keyname;
@end
