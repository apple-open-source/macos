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
 * LUArray_LDAP.m
 * Chain of LDAP entries
 * Copyright (C) 1997 Luke Howard. All rights reserved.
 * Luke Howard, March 1997.
 */

#import "LUAgent.h"
#import "LUArray_LDAP.h"
#import "LULDAPDictionary.h"
#import "LDAPAgent.h"
#import "LDAPAgent_Parsing.h"

/*
 *  LDAPMessage memory management is implemented in the following
 *  manner.
 *
 *  An instance of LULDAPArray "owns" the LDAPMessage chain;
 *  instances of LULDAPDictionary have a pointer into the
 *  chain which they DO NOT own.
 *
 *  Thus, every instance of LULDAPDictionary must be associated
 *  with an LULDAPArray; even dictionaries returned by non-
 *  enumeration methods.
 *
 *  When the dictionaries are retained by the "mother" array 
 *  such that the array is not dealloced until the dictionaries
 *  have, there are no potential problems.
 *
 *  If the array is dealloced before a dictionary which points
 *  into it is, there are potential problems. However, once
 *  the dictionaries have left the scope of LDAPAgent (in
 *  which the mother arrays are not explicitly released) they
 *  are ostensible instances of LUDictionary. Consequently, there is
 *  no way for the LDAPMessage pointer to be referenced
 *  within the body of lookupd. (The pointer is essentially
 *  used for convenience, in "binding" LDAP attributes to
 *  dictionary keys.) Obviously, -[LULDAPDictionary dealloc]
 *  doesn't touch the message pointer; -[LULDAPArray dealloc]
 *  frees the chain.
 *
 *  For the record, an alternative and slightly clearer method
 *  would involve each dictionary owning its own chain, which
 *  would be built at init time. The ldap_add_result_entry()
 *  function could be used to populate the chain with its
 *  sole entry.
 *
 *  Changes: August 1997
 *  Did away with autoreleasepools. Thus the array is freed in
 *  firstEntry: not by the autoreleasepool.
 *
 *  Changes: October 1997
 *  Fixed memory leak. If the parser returns nil (usually because
 *  ASSERT_TRUE() fails) then we remember to free the dictionary.
 *  Arguably the parser should do this itself, but it was much simpler
 *  to put this in LULDAPArray.
 *
 *  Changes: January 1998
 *  Fixed another memory leak. Classes which return nil in init should
 *  dealloc (or, if they're retained by the watchdog, release)
 *  themselves before returning nil. LDAPAgent needed to be more
 *  careful in checking return values from init. 
 *
 *  Changes: February 1998
 *  Now that categories are supported, all the parsing is done in the
 *  designated initializer. The advantage of this is that we never need
 *  to keep an array with unparsable entries around for very long. 
 *
 *  Changed the array from a subclass to a category.
 *  *AND* remembered to call [self init] instead of [super init].
 */

@implementation LUArray (LDAP)

- (LUArray *)initWithLDAPEntry:(LDAPMessage *)chain
	agent:(id)agent
	category:(LUCategory)cat
	stamp:(BOOL)dostamp
{
	LDAPMessage *e;

	[self init];

	if (chain == NULL)
	{
		[self release];
		return nil;
	}

	e = ldap_first_entry([agent session], chain);
	if (e == NULL)
	{
		[self release];
		ldap_msgfree(chain);
		return nil; /* no use returning a 0-element array */
	}
	
	do
	{
		LULDAPDictionary *ldapDict;

		ldapDict = [[LULDAPDictionary alloc] initWithEntry:e agent:agent];
		if (ldapDict != nil)
		{
			LUDictionary *parsedDict;
			
			parsedDict = [agent parse:ldapDict category:cat];
			
			if (parsedDict != nil)
			{
				[self addObject:parsedDict];
				
				if (parsedDict != ldapDict)
					[parsedDict release];	
			}
			[ldapDict release];
		}		
		e = ldap_next_entry([agent session], e);
	}
	while (e != NULL);

	if (count == 0)
	{
		[self release];
		ldap_msgfree(chain);
		return nil;
	}

	/*
	 * Make the validation stamp.
	 */
	if (dostamp)
	{
		LUDictionary *vstamp;
		char scratch[256];
		char ts[32];

		sprintf(scratch, "LDAPAgent: all %s", [LUAgent categoryName:cat]);
		[self setBanner:scratch];

		vstamp = [[LUDictionary alloc] init];
		[vstamp setBanner:"LDAPAgent validation stamp"];
		[vstamp setValue:"LDAP" forKey:"_lookup_info_system"];
		sprintf(ts, "%lu", time(0));	
		[vstamp setValue:ts forKey:"_lookup_LDAP_timestamp"];
		[vstamp setValue:[agent searchBaseForCategory:cat] forKey:"_lookup_LDAP_dn"];
		[self addValidationStamp: vstamp];
		[vstamp release];
	}

	ldap_msgfree(chain);

	return self;
}

@end

