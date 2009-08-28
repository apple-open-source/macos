/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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


//
// tokencache - persistent (on-disk) hardware token directory
//
// Here's the basic disk layout, rooted at /var/db/TokenCache (or $TOKENCACHE):
//  TBA
//
#include "tokencache.h"
#include <security_utilities/unix++.h>
#include <pwd.h>
#include <grp.h>

using namespace UnixPlusPlus;


//
// Here are the uid/gid values we assign to token daemons and their cache files
//
#define TOKEND_UID			"tokend"
#define TOKEND_GID			"tokend"
#define TOKEND_UID_FALLBACK	uid_t(-2)
#define TOKEND_GID_FALLBACK	gid_t(-2)


//
// Fixed relative file paths
//

// relative to cache root (use cache->path())
static const char configDir[] = "config";
static const char lastSSIDFile[] = "config/lastSSID";
static const char tokensDir[] = "tokens";

// relative to token directory (use token->path())
static const char ssidFile[] = "SSID";
static const char workDir[] = "work";
static const char cacheDir[] = "cache";


//
// Internal file I/O helpers. These read/write entire files.
// Note that the defaulted read functions do NOT write the default
// to disk; they work fine in read-only disk areas.
//
static unsigned long getFile(const string &path, unsigned long defaultValue)
{
	try {
		AutoFileDesc fd(path, O_RDONLY, FileDesc::modeMissingOk);
		if (fd) {
			string s; fd.readAll(s);
			unsigned long value; sscanf(s.c_str(), "%lu", &value);
			return value;
		}
	} catch (...) {
	}
	return defaultValue;
}

static string getFile(const string &path, const string &defaultValue)
{
	try {
		AutoFileDesc fd(path, O_RDONLY, FileDesc::modeMissingOk);
		if (fd) {
			string s; fd.readAll(s);
			return s;
		}
	} catch (...) {
	}
	return defaultValue;
}


static void putFile(const string &path, uint32 value)
{
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%ld\n", value);
	AutoFileDesc(path, O_WRONLY | O_CREAT | O_TRUNC).writeAll(buffer);
}

static void putFile(const string &path, const string &value)
{
	AutoFileDesc(path, O_WRONLY | O_CREAT | O_TRUNC).writeAll(value);
}


//
// The "rooted tree" utility class
//
void Rooted::root(const string &r)
{
	assert(mRoot.empty());	// can't re-set this
	mRoot = r;
}

string Rooted::path(const char *sub) const
{
	if (sub == NULL)
		return mRoot;
	return mRoot + "/" + sub;
}


//
// Open a TokenCache.
// If the cache does not exist at the path given, initialize it.
// If that fails, throw an exception.
//
TokenCache::TokenCache(const char *where)
	: Rooted(where), mLastSubservice(0)
{
	makedir(root(), O_CREAT, 0711, securityd);
	makedir(path(configDir), O_CREAT, 0700, securityd);
	makedir(path(tokensDir), O_CREAT, 0711, securityd);
	
	mLastSubservice = getFile(path(lastSSIDFile), 1);
	
	// identify uid/gid for token daemons
	struct passwd *pw = getpwnam(TOKEND_UID);
	mTokendUid = pw ? pw->pw_uid : TOKEND_UID_FALLBACK;
	struct group *gr = getgrnam(TOKEND_GID);
	mTokendGid = gr ? gr->gr_gid : TOKEND_GID_FALLBACK;
	
	secdebug("tokencache", "token cache rooted at %s (last ssid=%ld, uid/gid=%d/%d)",
		root().c_str(), mLastSubservice, mTokendUid, mTokendGid);
}

TokenCache::~TokenCache()
{
}


//
// Get a new, unused subservice id number.
// Update the tracking file so we won't hand it out again (ever) within this cache.
//
uint32 TokenCache::allocateSubservice()
{
	putFile(path(lastSSIDFile), ++mLastSubservice);
	return mLastSubservice;
}


//
// A slightly souped-up UnixPlusPlus::makedir
//
void TokenCache::makedir(const char *path, int flags, mode_t mode, Owner owner)
{
	UnixPlusPlus::makedir(path, flags, mode);
	switch(owner) {
	case securityd:
		// leave it alone; we own it alrady
		break;
	case tokend:
		::chown(path, tokendUid(), tokendGid());
		break;
	}
}


//
// Make a cache entry from a valid tokenUid.
// This will locate an existing entry or make a new one.
//
TokenCache::Token::Token(TokenCache &c, const string &tokenUid)
	: Rooted(c.path(string(tokensDir) + "/" + tokenUid)), cache(c)
{
	cache.makedir(root(), O_CREAT, 0711, securityd);
	if (mSubservice = getFile(path(ssidFile), 0)) {
		secdebug("tokencache", "found token \"%s\" ssid=%ld", tokenUid.c_str(), mSubservice);
		init(existing);
	} else {
		mSubservice = cache.allocateSubservice();   // allocate new, unique ssid...
		putFile(path(ssidFile), mSubservice);			// ... and save it in cache
		secdebug("tokencache", "new token \"%s\" ssid=%ld", tokenUid.c_str(), mSubservice);
		init(created);
	}
}


//
// Make a cache entry that is temporary and will never be reused.
//
TokenCache::Token::Token(TokenCache &c)
	: cache(c)
{
	mSubservice = cache.allocateSubservice();	// new, unique id
	char rootForm[30]; snprintf(rootForm, sizeof(rootForm),
		"%s/temporary:%ld", tokensDir, mSubservice);
	root(cache.path(rootForm));
	cache.makedir(root(), O_CREAT | O_EXCL, 0711, securityd);
	putFile(path(ssidFile), mSubservice);			// ... and save it in cache
	secdebug("tokencache", "temporary token \"%s\" ssid=%ld", rootForm, mSubservice);
	init(temporary);
}


//
// Common constructor setup code
//
void TokenCache::Token::init(Type type)
{
	mType = type;
	cache.makedir(workPath(), O_CREAT, 0700, tokend);
	cache.makedir(cachePath(), O_CREAT, 0700, tokend);
}


//
// The Token destructor might clean or preen a bit, but shouldn't take
// too long (or too much effort).
//
TokenCache::Token::~Token()
{
	if (type() == temporary)
		secdebug("tokencache", "@@@ should delete the cache directory here...");
}


//
// Attributes of TokenCache::Tokens
//
string TokenCache::Token::workPath() const
{
	return path("Work");
}

string TokenCache::Token::cachePath() const
{
	return path("Cache");
}


string TokenCache::Token::printName() const
{
	return getFile(path("PrintName"), "");
}

void TokenCache::Token::printName(const string &name)
{
	putFile(path("PrintName"), name);
}
