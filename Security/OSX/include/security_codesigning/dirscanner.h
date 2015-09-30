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

#ifndef _H_DIRSCANNER
#define _H_DIRSCANNER

#include "resources.h"
#include <dirent.h>
#include <fts.h>
#include <security_utilities/cfutilities.h>

namespace Security {
namespace CodeSigning {


class DirScanner {
public:
	DirScanner(const char *path);
	DirScanner(string path);
	~DirScanner();

	struct dirent *getNext();	// gets the next item out of this DirScanner
	bool initialized();			// returns false if the constructor failed to initialize the dirent

private:
	string path;
	DIR *dp = NULL;
	void initialize();
	bool init;
};


class DirValidator {
public:
	DirValidator() : mRequireCount(0) { }
	~DirValidator();

	enum {
		file = 0x01,
		directory = 0x02,
		symlink = 0x04,
		noexec = 0x08,
		required = 0x10,
		descend = 0x20,
	};

	typedef std::string (^TargetPatternBuilder)(const std::string &name, const std::string &target);

private:
	class Rule : public ResourceBuilder::Rule {
	public:
		Rule(const std::string &pattern, uint32_t flags, TargetPatternBuilder targetBlock);
		~Rule();

		bool matchTarget(const char *path, const char *target) const;

	private:
		TargetPatternBuilder mTargetBlock;
	};
	void addRule(Rule *rule) { mRules.push_back(rule); }

	class FTS {
	public:
		FTS(const std::string &path, int options = FTS_PHYSICAL | FTS_COMFOLLOW | FTS_NOCHDIR);
		~FTS();

		operator ::FTS* () const { return mFTS; }

	private:
		::FTS *mFTS;
	};

public:
	void allow(const std::string &namePattern, uint32_t flags, TargetPatternBuilder targetBlock = NULL)
	{ addRule(new Rule(namePattern, flags, targetBlock)); }
	void require(const std::string &namePattern, uint32_t flags, TargetPatternBuilder targetBlock = NULL)
	{ addRule(new Rule(namePattern, flags | required, targetBlock)); mRequireCount++; }

	void allow(const std::string &namePattern, uint32_t flags, std::string targetPattern)
	{ allow(namePattern, flags, ^ string (const std::string &name, const std::string &target) { return targetPattern; }); }
	void require(const std::string &namePattern, uint32_t flags, std::string targetPattern)
	{ require(namePattern, flags, ^ string (const std::string &name, const std::string &target) { return targetPattern; }); }

	void validate(const std::string &root, OSStatus error);

private:
	Rule * match(const char *relpath, uint32_t flags, bool executable, const char *target = NULL);

private:
	typedef std::vector<Rule *> Rules;
	Rules mRules;
	int mRequireCount;
};


} // end namespace CodeSigning
} // end namespace Security

#endif // !_H_DIRSCANNER
