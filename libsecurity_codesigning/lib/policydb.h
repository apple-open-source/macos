/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
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
#ifndef _H_POLICYDB
#define _H_POLICYDB

#include <security_utilities/globalizer.h>
#include <security_utilities/hashing.h>
#include <security_utilities/sqlite++.h>
#include <CoreFoundation/CoreFoundation.h>

namespace Security {
namespace CodeSigning {


namespace SQLite = SQLite3;


static const char defaultDatabase[] = "/var/db/SystemPolicy";
static const char visibleSecurityFlagFile[] = "/var/db/.sp_visible";

static const double never = 5000000;	// expires never (i.e. in the year 8977)


typedef SHA1::SDigest ObjectHash;


typedef uint AuthorityType;
enum {
	kAuthorityInvalid = 0,				// not a valid authority type
	kAuthorityExecute = 1,				// authorizes launch and execution
	kAuthorityInstall = 2,				// authorizes installation
	kAuthorityOpenDoc = 3,				// authorizes opening of documents
};


//
// Defined flags for authority flags column
//
enum {
	kAuthorityFlagVirtual =	0x0001,	// virtual rule (anchoring object records)
	kAuthorityFlagDefault =	0x0002,	// rule is part of the original default set
	kAuthorityFlagInhibitCache = 0x0004, // never cache outcome of this rule
};


//
// Mapping/translation to/from API space
//
AuthorityType typeFor(CFDictionaryRef context, AuthorityType type = kAuthorityInvalid);


//
// An open policy database.
// Usually read-only, but can be opened for write by privileged callers.
// This is a translucent wrapper around SQLite::Database; the caller
// is expected to work with statement rows.
//
class PolicyDatabase : public SQLite::Database {
public:
	PolicyDatabase(const char *path = NULL,	int flags = SQLITE_OPEN_READONLY);
	virtual ~PolicyDatabase();
	
public:
	bool checkCache(CFURLRef path, AuthorityType type, CFMutableDictionaryRef result);

public:
	void purgeAuthority();
	void purgeObjects();
	void purgeObjects(double priority);
};


//
// Check the system-wide overriding flag file
//
bool overrideAssessment();


} // end namespace CodeSigning
} // end namespace Security

#endif //_H_POLICYDB
