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
#include "cs.h"
#include "policydb.h"
#include "policyengine.h"
#include <Security/CodeSigning.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/cfmunge.h>

namespace Security {
namespace CodeSigning {


using namespace SQLite;


//
// The one and only PolicyDatabase object.
// It auto-adapts to readonly vs. writable use.
//
ModuleNexus<PolicyDatabase> PolicyDatabase;


//
// Determine the database path
//
static const char *dbPath()
{
	if (const char *s = getenv("SYSPOLICYDATABASE"))
		return s;
	return defaultDatabase;
}


//
// Open the database (creating it if necessary and possible).
// Note that this isn't creating the schema; we do that on first write.
//
PolicyDatabase::PolicyDatabase(const char *path, int flags)
	: SQLite::Database(path ? path : dbPath(), flags)
{
}

PolicyDatabase::~PolicyDatabase()
{ /* virtual */ }


//
// Quick-check the cache for a match.
// Return true on a cache hit, false on failure to confirm a hit for any reason.
//
bool PolicyDatabase::checkCache(CFURLRef path, AuthorityType type, CFMutableDictionaryRef result)
{
	// we currently don't use the cache for anything but execution rules
	if (type != kAuthorityExecute)
		return false;
	
	SecCSFlags validationFlags = kSecCSDefaultFlags;
	if (overrideAssessment())	// we'll force the verdict to 'pass' at the end, so don't sweat validating code
		validationFlags = kSecCSBasicValidateOnly;

	CFRef<SecStaticCodeRef> code;
	MacOSError::check(SecStaticCodeCreateWithPath(path, kSecCSDefaultFlags, &code.aref()));
	if (SecStaticCodeCheckValidity(code, validationFlags, NULL) != noErr)
		return false;	// quick pass - any error is a cache miss
	CFRef<CFDictionaryRef> info;
	MacOSError::check(SecCodeCopySigningInformation(code, kSecCSDefaultFlags, &info.aref()));
	CFDataRef cdHash = CFDataRef(CFDictionaryGetValue(info, kSecCodeInfoUnique));
	
	// check the cache table for a fast match
	SQLite::Statement cached(*this, "SELECT allow, expires, label, authority FROM object WHERE type = ?1 and hash = ?2;");
	cached.bind(1).integer(type);
	cached.bind(2) = cdHash;
	if (cached.nextRow()) {
		bool allow = int(cached[0]);
		const char *label = cached[2];
		SQLite::int64 auth = cached[3];
		bool valid = true;
		if (SQLite3::int64 expires = cached[1])
			valid = time(NULL) <= expires;
		if (valid) {
			SYSPOLICY_ASSESS_CACHE_HIT();
			cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, allow);
			PolicyEngine::addAuthority(result, label, auth, kCFBooleanTrue);
			return true;
		}
	}
	return false;
}


//
// Purge the object cache of all expired entries
//
void PolicyDatabase::purge(const char *table)
{
	SQLite::Statement cleaner(*this,
		"DELETE FROM ?1 WHERE expires < DATE_TIME('now');");
	cleaner.bind(1) = table;
	cleaner.execute();
}


//
// Check the override-enable master flag
//
bool overrideAssessment()
{
	if (::access(visibleSecurityFlagFile, F_OK) == 0) {
		return false;
	} else if (errno == ENOENT) {
		return true;
	} else
		UnixError::throwMe();
}


} // end namespace CodeSigning
} // end namespace Security
