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
 * LULDAPDictionary.m
 * Abstraction of LDAP entry
 * Copyright (C) 1997 Luke Howard. All rights reserved.
 * Luke Howard, March 1997.
 */

#import <stdlib.h>
#import <sys/param.h>
#import <string.h>
#import <sys/time.h>
#import <NetInfo/dsutil.h>

#import "LULDAPDictionary.h"
#import "LDAPAgent.h"
#import "LDAPAttributes.h"

#ifdef OIDTABLE
#define CryptPrefixLength	(strlen(CryptPrefix))
#else
#define CryptPrefixLength	(sizeof(CryptPrefix) - 1)
#endif

#define CryptPrefix		NameForKey(OID_CRYPT)
#define HasCryptPrefix(s)	(strncasecmp(s, CryptPrefix, CryptPrefixLength) == 0)

/*
 *  LULDAPDictionary is an abstraction of an LDAP entry which
 *  will be ultimately manipulated into an LUDictionary.
 *
 *  As such, it is a subclass of LUDictionary.m
 *
 *  The dictionary is initialised with a pointer into a result
 *  chain (returned from one of the LDAP API search functions).
 *  This function is NOT owned by the instance; typically, it's
 *  owned by an instance of LULDAPArray.
 *
 *   The manipulation of the dictionary involves "binding"
 *   LDAP attributes to LUDictionary keys.
 *
 *   There are a number of different binding methods:
 *
 *   bindRdnToName:	binds the entry's relative distinguished
 *			name to the "name" dictionary key.
 *			If the value cannot be found within the RDN,
 *			the entry itself is searched (note the LDAP
 *			server does not guarantee value ordering).
 *
 *   bindAllAttributesExcept:	used where there is a 1:1 mapping between
 *			LDAP attribute types and dictionary keys, with
 *			the exception of one (ie. that CN is used for
 *			the name key).
 *
 *   bindAttribute:toKey:	binds all attribute values to a key
 *
 *   bindAttribute:toKey:exceptValue:	binds an attribute, omitting
 *			the specified value, which will typically have been
 *			already bound with another method.
 *
 *   bindAttributeCrypted:toKey:	binds an attribute whose syntax is
 *			{crypt}string, where {crypt} is stripped. All
 *			attribute values are tried until one (or no)
 *			value matches syntax.
 *
 *   All the bind methods return YES if the bind was successful.
 *   The caller may use this to determine whether the entry
 *   was parsable or not.
 */

@implementation LULDAPDictionary

- (id)initWithEntry:(LDAPMessage *)anEntry agent:(id)source;
{
	char ts[32];

	[super init];

	entry = anEntry;

	dn = ldap_get_dn([source session], entry);
	if (dn == NULL)
	{
		[self release];
		return nil;
	}

	[self setAgent:source];

	[self setValue:"LDAP" forKey:"_lookup_info_system"];
	[self setValue:dn forKey:"_lookup_LDAP_dn"];

	/* this is used for cache validation */
	(void)[self bindAttribute:OID_MODIFYTIMESTAMP toKey:"_lookup_LDAP_modify_timestamp"];
	(void)[self bindAttribute:OID_TTL toKey:"_lookup_LDAP_time_to_live"];

	sprintf(ts, "%lu", time(0));	
	[self setValue:ts forKey:"_lookup_LDAP_timestamp"];

	return self;
}

- (void)dealloc
{
	ldap_memfree(dn);
	dn = NULL;
	
	[super dealloc];
}

- (char **)entryValuesForAttribute:(oid_name_t)attrname
{
	/* caller frees values */
	char **v;

	v = ldap_get_values([[self agent] session], entry, NameForKey(attrname));

	return v;
}

- (BOOL)bindAllAttributesExcept:(oid_name_t)attrname
{
	BerElement *ptr;
	char *attr;
	BOOL bRes = YES;
	LDAP *ld = [agent session];

	for (attr = ldap_first_attribute(ld, entry, &ptr);
		attr != NULL;
		attr = ldap_next_attribute(ld, entry, ptr)
		)
	{
		/* omit objectclass: it's not relavent to lookupd. */
		char **values;
		
		if (!strcasecmp(attr, NameForKey(OID_OBJECTCLASS)) || !strcasecmp(NameForKey(attrname), attr))
		{
			continue;
		}
		
		values = ldap_get_values([[self agent] session], entry, attr);
		if (values != NULL)
		{
			[self setValues:values forKey:attr];
			ldap_value_free(values);
		}
		
		/* V3 API returns allocated memory */
		free(attr);
	}

	ldap_ber_free(ptr, 0);

	return bRes;
}

- (BOOL)bindAttributeCrypted:(oid_name_t)attrname
			toKey:(char *)keyname
{
	char **asPasswd;
	char **pwd;
	BOOL bRes = NO;

	asPasswd = [self entryValuesForAttribute:attrname];
	if (asPasswd == NULL)
	{
		return bRes;
	}

	for (pwd = asPasswd; *pwd != NULL; pwd++)
	{
		if (HasCryptPrefix(*pwd))
		{
			[self setValue:(*pwd + CryptPrefixLength) forKey: keyname];
			bRes = YES;
			break;
		}
	}

	ldap_value_free(asPasswd);
	
	return bRes;
}

- (BOOL)bindAttribute:(oid_name_t)attrname
			toKey:(char *)keyname
{
	char **v;

	v = [self entryValuesForAttribute:attrname];
	if (v == NULL)
	{
		return NO;
	}

	[self setValues:v forKey:keyname];
	ldap_value_free(v);
	
	return YES;
}

- (BOOL)bindAttribute:(oid_name_t)attrname
			toKey:(char *)keyname
			exceptValue:(char *)value
{
	/* use -[LUDictionary merge...] here? */
	char **v;
	char **vp;

	v = [self entryValuesForAttribute:attrname];
	if (v == NULL)
	{
		return NO;
	}
	
	for (vp = v; *vp != NULL; vp++)
	{
		if (!streq(value, *vp))
		{
			[self addValue:*vp forKey:keyname];
		}
	}
	ldap_value_free(v);
	return YES;
}

- (BOOL)bindRdnToName:(oid_name_t)attr
{
	char **exploded_dn;
	char *rdnvalue = NULL;

	exploded_dn = ldap_explode_dn(dn, 0);

	if (exploded_dn != NULL)
	{
		char **exploded_rdn;
		int len = strlen(NameForKey(attr));
#ifdef OIDTABLE
		char *ava = (char *)malloc(len + 2);
#else
		/*
		 * We know at compile time the length of ze attributes,
		 * so it's probably safe to allocate off the stack.
		 * Yet another useless optimization.
		 */
		char *ava = (char *)alloca(len + 2);
#endif /* OIDTABLE */

		strncpy(ava, NameForKey(attr), len);
		ava[len] = '=';
		ava[++len] = '\0';

		exploded_rdn = ldap_explode_rdn(exploded_dn[0], 0);
		if (exploded_rdn != NULL)
		{
			char **p;

			for (p = exploded_rdn; *p != NULL; p++)
			{
				if (strncasecmp(*p, ava, len) == 0)
				{
					rdnvalue = copyString(*p + len);
					break;
				}
			}
			ldap_value_free(exploded_rdn);
		}
		ldap_value_free(exploded_dn);
#ifdef OIDTABLE
		free(ava);
#endif
	}

	if (rdnvalue == NULL)
	{
		/* if the RDN isn't of attribute type attr, then we need to
		 * take the first attribute value of that type. It is
		 * possible that the value won't be the "distinguished"
		 * value of the entry.
		 */
		char **v;

		v = [self entryValuesForAttribute:attr];
		if (v != NULL)
		{
			[self setValue:v[0] forKey:"name"];
			ldap_value_free(v);
			return YES;
		}
		else
		{
			return NO;
		}
	}

	[self setValue:rdnvalue forKey:"name"];
	free(rdnvalue);

	return YES;
}

@end
