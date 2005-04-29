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
// bundlerepository - directory search paths for bundles
//
#ifndef _H_BUNDLEREPOSITORY
#define _H_BUNDLEREPOSITORY

#include <security_utilities/cfutilities.h>
#include <security_utilities/refcount.h>
#include <security_utilities/osxcode.h>
#include <CoreFoundation/CoreFoundation.h>
#include <string>
#include <vector>


namespace Security {


//
// PathList abstracts a directory search path.
// It's not really powerful enough to be useful on its own.
//
class PathList {
public:
	PathList();
	PathList(const string &subPath, const char *suffix = NULL,
		const char *envar = NULL, bool forUser = true);
	virtual ~PathList();
	
	void addDirectory(const string &dirPath);

protected:
	vector<string> mPaths;
	string mSuffix;
	IFDEBUG(string mDebugOverride);
};


//
// CodeRepository<Code> represents all code objects within the PathList search path,
// represented forcibly as objects of type Code.
//
template <class Code>
class CodeRepository : public vector<RefPointer<Code> >, public PathList {
public:
	CodeRepository() { }				// empty - populate with paths
	CodeRepository(const string &subPath, const char *suffix = NULL,
		const char *envar = NULL, bool forUser = true)
		: PathList(subPath, suffix, envar, forUser) { }
	
	void update();
};

} // end namespace Security

#endif //_H_BUNDLEREPOSITORY
