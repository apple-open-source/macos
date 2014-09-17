/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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

#include <dirent.h>
#include <unistd.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/debugging.h>
#include "dirscanner.h"

namespace Security {
namespace CodeSigning {


DirScanner::DirScanner(const char *path)
	: init(false)
{
	this->path = std::string(path);
	this->initialize();
}

DirScanner::DirScanner(string path)
	: init(false)
{
	this->path = path;
	this->initialize();
}

DirScanner::~DirScanner()
{
        if (this->dp != NULL)
                (void) closedir(this->dp);
}

void DirScanner::initialize()
{
	if (this->dp == NULL) {
		errno = 0;
		if ((this->dp = opendir(this->path.c_str())) == NULL) {
			if (errno == ENOENT) {
				init = false;
			} else {
				UnixError::check(-1);
			}
		} else
			init = true;
	} else
		MacOSError::throwMe(errSecInternalError);
}

struct dirent * DirScanner::getNext()
{
	return readdir(this->dp);
}

bool DirScanner::initialized()
{
	return this->init;
}


DirValidator::~DirValidator()
{
	for (Rules::iterator it = mRules.begin(); it != mRules.end(); ++it)
		delete *it;
}

void DirValidator::validate(const string &root, OSStatus error)
{
	std::set<Rule *> reqMatched;
	FTS fts(root);
	while (FTSENT *ent = fts_read(fts)) {
		const char *relpath = ent->fts_path + root.size() + 1;	// skip prefix + "/"
		bool executable = ent->fts_statp->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH);
		Rule *rule = NULL;
		switch (ent->fts_info) {
		case FTS_F:
			secdebug("dirval", "file %s", ent->fts_path);
			rule = match(relpath, file, executable);
			break;
		case FTS_SL: {
			secdebug("dirval", "symlink %s", ent->fts_path);
			char target[PATH_MAX];
			ssize_t len = ::readlink(ent->fts_accpath, target, sizeof(target)-1);
			if (len < 0)
				UnixError::throwMe();
			target[len] = '\0';
			rule = match(relpath, symlink, executable, target);
			break;
		}
		case FTS_D:
			secdebug("dirval", "entering %s", ent->fts_path);
			if (ent->fts_level == FTS_ROOTLEVEL)
				continue;	// skip root directory
			rule = match(relpath, directory, executable);
			if (!rule || !(rule->flags & descend))
				fts_set(fts, ent, FTS_SKIP);	// do not descend
			break;
		case FTS_DP:
			secdebug("dirval", "leaving %s", ent->fts_path);
			continue;
		default:
			secdebug("dirval", "type %d (errno %d): %s", ent->fts_info, ent->fts_errno, ent->fts_path);
			MacOSError::throwMe(error);	 // not a file, symlink, or directory
		}
		if (!rule)
			MacOSError::throwMe(error);	 // no match
		else if (rule->flags & required)
			reqMatched.insert(rule);
	}
	if (reqMatched.size() != mRequireCount) {
		secdebug("dirval", "matched %d of %d required rules", reqMatched.size(), mRequireCount);
		MacOSError::throwMe(error);		 // not all required rules were matched
	}
}

DirValidator::Rule * DirValidator::match(const char *path, uint32_t flags, bool executable, const char *target)
{
	for (Rules::iterator it = mRules.begin(); it != mRules.end(); ++it) {
		Rule *rule = *it;
		if ((rule->flags & flags)
		    && !(executable && (rule->flags & noexec))
		    && rule->match(path)
		    && (!target || rule->matchTarget(path, target)))
			return rule;
	}
	return NULL;
}

DirValidator::FTS::FTS(const string &path, int options)
{
	const char * paths[2] = { path.c_str(), NULL };
	mFTS = fts_open((char * const *)paths, options, NULL);
	if (!mFTS)
		UnixError::throwMe();
}

DirValidator::FTS::~FTS()
{
	fts_close(mFTS);
}

DirValidator::Rule::Rule(const string &pattern, uint32_t flags, TargetPatternBuilder targetBlock)
	: ResourceBuilder::Rule(pattern, 0, flags), mTargetBlock(NULL)
{
	if (targetBlock)
		mTargetBlock = Block_copy(targetBlock);
}

DirValidator::Rule::~Rule()
{
	if (mTargetBlock)
		Block_release(mTargetBlock);
}

bool DirValidator::Rule::matchTarget(const char *path, const char *target) const
{
	if (!mTargetBlock)
		MacOSError::throwMe(errSecCSInternalError);
	string pattern = mTargetBlock(path, target);
	if (pattern.empty())
		return true;	// always match empty pattern
	secdebug("dirval", "%s: match target %s against %s", path, target, pattern.c_str());
	regex_t re;
	if (::regcomp(&re, pattern.c_str(), REG_EXTENDED | REG_NOSUB))
		MacOSError::throwMe(errSecCSInternalError);
	switch (::regexec(&re, target, 0, NULL, 0)) {
	case 0:
		return true;
	case REG_NOMATCH:
		return false;
	default:
		MacOSError::throwMe(errSecCSInternalError);
	}
}


} // end namespace CodeSigning
} // end namespace Security
