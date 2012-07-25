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
// CodeRepository - directory search paths for bundles
//
#include <security_utilities/coderepository.h>
#include <security_utilities/debugging.h>


namespace Security {


PathList::PathList()
{
}


PathList::PathList(const string &subPath,
		const char *suffix /* = NULL */,
		const char *envar /* = NULL */,
		bool forUser /* = true */)
	: mSuffix(suffix)
{
	if (envar)
		if (const char *envPath = getenv(envar)) {
#if !defined(NDEBUG)
			if (envPath[0] == '!') {
				// envar="!path" -> single-item override (debugging only)
				mDebugOverride = envPath + 1;
				secdebug("pathlist", "%p env(\"%s\") overrides to \"%s\"",
					this, envar, mDebugOverride.c_str());
				return;
			}
#endif //NDEBUG

			// treat envPath as a classic colon-separated list of directories
			secdebug("pathlist", "%p configuring from env(\"%s\")", this, envar);
			while (const char *p = strchr(envPath, ':')) {
				addDirectory(string(envPath, p - envPath));
				envPath = p + 1;
			}
			addDirectory(envPath);
			return;
		}

	// no joy from environment variables
	secdebug("pathlist", "%p configuring from default path set \"%s\"", this, subPath.c_str());
	if (forUser)
		secdebug("pathlist", "user search list not yet implemented");
	addDirectory("/Library/" + subPath);
	addDirectory("/System/Library/" + subPath);
}


PathList::~PathList()
{
	// virtual
}


void PathList::addDirectory(const string &dirPath)
{
	//@@@ validate dirPath?
	mPaths.push_back(dirPath);
}


}	// end namespace Security
