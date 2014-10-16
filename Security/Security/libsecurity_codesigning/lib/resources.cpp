/*
 * Copyright (c) 2006-2014 Apple Inc. All Rights Reserved.
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
// resource directory construction and verification
//
#include "resources.h"
#include "csutilities.h"
#include <security_utilities/unix++.h>
#include <security_utilities/debugging.h>
#include <Security/CSCommon.h>
#include <security_utilities/unix++.h>
#include <security_utilities/cfmunge.h>

// These are pretty nasty, but are a quick safe fix
// to pass information down to the gatekeeper collection tool
extern "C" {
    int GKBIS_DS_Store_Present;
	int GKBIS_Dot_underbar_Present;
	int GKBIS_Num_localizations;
	int GKBIS_Num_files;
	int GKBIS_Num_dirs;
	int GKBIS_Num_symlinks;
}

namespace Security {
namespace CodeSigning {


static string removeTrailingSlash(string path)
{
	if (path.substr(path.length()-2, 2) == "/.")
		return path.substr(0, path.length()-2);
	else if (path.substr(path.length()-1, 1) == "/")
		return path.substr(0, path.length()-1);
	else
		return path;
}

//
// Construction and maintainance
//
ResourceBuilder::ResourceBuilder(const std::string &root, const std::string &relBase,
	CFDictionaryRef rulesDict, CodeDirectory::HashAlgorithm hashType, bool strict, const MacOSErrorSet& toleratedErrors)
	: mHashType(hashType),
	  mCheckUnreadable(strict && toleratedErrors.find(errSecCSSignatureNotVerifiable) == toleratedErrors.end()),
	  mCheckUnknownType(strict && toleratedErrors.find(errSecCSResourceNotSupported) == toleratedErrors.end())
{
	assert(!root.empty());
	mRoot = removeTrailingSlash(root);
	mRelBase = removeTrailingSlash(relBase);
	if (mRoot != mRelBase && mRelBase != mRoot + "/Contents")
		MacOSError::throwMe(errSecCSInternalError);
	const char * paths[2] = { mRoot.c_str(), NULL };
	mFTS = fts_open((char * const *)paths, FTS_PHYSICAL | FTS_COMFOLLOW | FTS_NOCHDIR, NULL);
	if (!mFTS)
		UnixError::throwMe();
	mRawRules = rulesDict;
	CFDictionary rules(rulesDict, errSecCSResourceRulesInvalid);
	rules.apply(this, &ResourceBuilder::addRule);
}

ResourceBuilder::~ResourceBuilder()
{
	for (Rules::iterator it = mRules.begin(); it != mRules.end(); ++it)
		delete *it;
	fts_close(mFTS);	// do not check error - it's not worth aborting over (double fault etc.)
}


//
// Parse and add one matching rule
//
void ResourceBuilder::addRule(CFTypeRef key, CFTypeRef value)
{
	string pattern = cfString(key, errSecCSResourceRulesInvalid);
	unsigned weight = 1;
	uint32_t flags = 0;
	if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
		if (value == kCFBooleanFalse)
			flags |= omitted;
	} else {
		CFDictionary rule(value, errSecCSResourceRulesInvalid);
		if (CFNumberRef weightRef = rule.get<CFNumberRef>("weight"))
			weight = cfNumber<unsigned int>(weightRef);
		if (CFBooleanRef omitRef = rule.get<CFBooleanRef>("omit"))
			if (omitRef == kCFBooleanTrue)
				flags |= omitted;
		if (CFBooleanRef optRef = rule.get<CFBooleanRef>("optional"))
			if (optRef == kCFBooleanTrue)
				flags |= optional;
		if (CFBooleanRef nestRef = rule.get<CFBooleanRef>("nested"))
			if (nestRef == kCFBooleanTrue)
				flags |= nested;
		if (CFBooleanRef topRef = rule.get<CFBooleanRef>("top"))
			if (topRef == kCFBooleanTrue)
				flags |= top;
	}
	addRule(new Rule(pattern, weight, flags));
}

static bool findStringEndingNoCase(const char *path, const char * end)
{
    size_t len_path = strlen(path);
    size_t len_end = strlen(end);
    
    if (len_path >= len_end) {
        return strcasecmp(path + (len_path - len_end), end) == 0;
    } else
        return false;
}

//
// Locate the next non-ignored file, look up its rule, and return it.
// Returns NULL when we're out of files.
//
void ResourceBuilder::scan(Scanner next)
{
	bool first = true;
    
	while (FTSENT *ent = fts_read(mFTS)) {
		static const char ds_store[] = ".DS_Store";
		const char *relpath = ent->fts_path + mRoot.size() + 1;	// skip prefix + "/"
		std::string rp;
		if (mRelBase != mRoot) {
			assert(mRelBase == mRoot + "/Contents");
			rp = "../" + string(relpath);
			if (rp.substr(0, 12) == "../Contents/")
				rp = rp.substr(12);
			relpath = rp.c_str();
		}
		switch (ent->fts_info) {
		case FTS_F:
			secdebug("rdirenum", "file %s", ent->fts_path);
			GKBIS_Num_files++;
                
			// These are checks for the gatekeeper collection
			static const char underbar[] = "._";
			if (strncasecmp(ent->fts_name, underbar, strlen(underbar)) == 0)
				GKBIS_Dot_underbar_Present++;

			if (strcasecmp(ent->fts_name, ds_store) == 0)
				GKBIS_DS_Store_Present++;

			if (Rule *rule = findRule(relpath))
				if (!(rule->flags & (omitted | exclusion)))
					next(ent, rule->flags, relpath, rule);
			break;
		case FTS_SL:
			// symlinks cannot ever be nested code, so quietly convert to resource file
			secdebug("rdirenum", "symlink %s", ent->fts_path);
			GKBIS_Num_symlinks++;

			if (strcasecmp(ent->fts_name, ds_store) == 0)
				MacOSError::throwMe(errSecCSDSStoreSymlink);
                
			if (Rule *rule = findRule(relpath))
				if (!(rule->flags & (omitted | exclusion)))
					next(ent, rule->flags & ~nested, relpath, rule);
			break;
		case FTS_D:
			secdebug("rdirenum", "entering %s", ent->fts_path);
			GKBIS_Num_dirs++;

			if (!first) {	// skip root directory (relpath invalid)
				if (Rule *rule = findRule(relpath)) {
					if (rule->flags & nested) {
						if (strchr(ent->fts_name, '.')) {	// nested, has extension -> treat as nested bundle
							next(ent, rule->flags, relpath, rule);
							fts_set(mFTS, ent, FTS_SKIP);
						}
					} else if (rule->flags & exclusion) {	// exclude the whole directory
						fts_set(mFTS, ent, FTS_SKIP);
					}
					// else treat as normal directory and descend into it
				}
			}
			// Report the number of localizations
			if (findStringEndingNoCase(ent->fts_name, ".lproj"))
				GKBIS_Num_localizations++;
			first = false;

			break;
		case FTS_DP:
			secdebug("rdirenum", "leaving %s", ent->fts_path);
			break;
		case FTS_DNR:
			secdebug("rdirenum", "cannot read directory %s", ent->fts_path);
			if (mCheckUnreadable)
				MacOSError::throwMe(errSecCSSignatureNotVerifiable);
			break;
		default:
			secdebug("rdirenum", "type %d (errno %d): %s",
				ent->fts_info, ent->fts_errno, ent->fts_path);
			if (mCheckUnknownType)
				MacOSError::throwMe(errSecCSResourceNotSupported);
			break;
		}
	}
}


//
// Check a single for for inclusion in the resource envelope
//
bool ResourceBuilder::includes(string path) const
{
	if (Rule *rule = findRule(path))
		return !(rule->flags & (omitted | exclusion));
	else
		return false;
}


//
// Find the best-matching resource rule for an alleged resource file.
// Returns NULL if no rule matches, or an exclusion rule applies.
//
ResourceBuilder::Rule *ResourceBuilder::findRule(string path) const
{
	Rule *bestRule = NULL;
	secdebug("rscan", "test %s", path.c_str());
	for (Rules::const_iterator it = mRules.begin(); it != mRules.end(); ++it) {
		Rule *rule = *it;
		secdebug("rscan", "try %s", rule->source.c_str());
		if (rule->match(path.c_str())) {
			secdebug("rscan", "match");
			if (rule->flags & exclusion) {
				secdebug("rscan", "excluded");
				return rule;
			}
			if (!bestRule || rule->weight > bestRule->weight)
				bestRule = rule;
		}
	}
	secdebug("rscan", "choosing %s (%d,0x%x)",
		bestRule ? bestRule->source.c_str() : "NOTHING",
		bestRule ? bestRule->weight : 0,
		bestRule ? bestRule->flags : 0);
	return bestRule;
}


//
// Hash a file and return a CFDataRef with the hash
//
CFDataRef ResourceBuilder::hashFile(const char *path) const
{
	UnixPlusPlus::AutoFileDesc fd(path);
	fd.fcntl(F_NOCACHE, true);		// turn off page caching (one-pass)
	MakeHash<ResourceBuilder> hasher(this);
	hashFileData(fd, hasher.get());
	Hashing::Byte digest[hasher->digestLength()];
	hasher->finish(digest);
	return CFDataCreate(NULL, digest, sizeof(digest));
}


//
// Regex matching objects
//
ResourceBuilder::Rule::Rule(const std::string &pattern, unsigned w, uint32_t f)
	: weight(w), flags(f), source(pattern)
{
	if (::regcomp(this, pattern.c_str(), REG_EXTENDED | REG_NOSUB))	//@@@ REG_ICASE?
		MacOSError::throwMe(errSecCSResourceRulesInvalid);
	secdebug("csresource", "%p rule %s added (weight %d, flags 0x%x)",
		this, pattern.c_str(), w, f);
}

ResourceBuilder::Rule::~Rule()
{
	::regfree(this);
}

bool ResourceBuilder::Rule::match(const char *s) const
{
	switch (::regexec(this, s, 0, NULL, 0)) {
	case 0:
		return true;
	case REG_NOMATCH:
		return false;
	default:
		MacOSError::throwMe(errSecCSResourceRulesInvalid);
	}
}


std::string ResourceBuilder::escapeRE(const std::string &s)
{
	string r;
	for (string::const_iterator it = s.begin(); it != s.end(); ++it) {
		char c = *it;
		if (strchr("\\[]{}().+*?", c))
			r.push_back('\\');
		r.push_back(c);
	}
	return r;
}


//
// Resource Seals
//
ResourceSeal::ResourceSeal(CFTypeRef it)
	: mDict(NULL), mHash(NULL), mRequirement(NULL), mLink(NULL), mFlags(0)
{
	if (it == NULL)
		MacOSError::throwMe(errSecCSResourcesInvalid);
	if (CFGetTypeID(it) == CFDataGetTypeID()) {
		mHash = CFDataRef(it);
	} else {
		int optional = 0;
		mDict = CFDictionaryRef(it);
		bool err;
		if (CFDictionaryGetValue(mDict, CFSTR("requirement")))
			err = !cfscan(mDict, "{requirement=%SO,?optional=%B}", &mRequirement, &optional);
		else if (CFDictionaryGetValue(mDict, CFSTR("symlink")))
			err = !cfscan(mDict, "{symlink=%SO,?optional=%B}", &mLink, &optional);
		else
			err = !cfscan(mDict, "{hash=%XO,?optional=%B}", &mHash, &optional);
		if (err)
			MacOSError::throwMe(errSecCSResourcesInvalid);
		if (optional)
			mFlags |= ResourceBuilder::optional;
		if (mRequirement)
			mFlags |= ResourceBuilder::nested;
	}
}


} // end namespace CodeSigning
} // end namespace Security
