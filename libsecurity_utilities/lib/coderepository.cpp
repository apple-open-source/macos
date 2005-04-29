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


//
// The generic implementation of update works with subclasses of GenericBundle,
// represented through CFBundleRefs collected via CFBundle.
// (Technically, this would work with anything that has a constructor from CFBundleRef.)
// If we ever wanted a CodeRepository<ExecutableTool>, we'd specialize update() to deal with
// ExecutableTool's slightly different constructor.
//
template <class Code>
void CodeRepository<Code>::update()
{
#if !defined(NDEBUG)
	if (!mDebugOverride.empty()) {
		erase(this->begin(), this->end());
		try {
			push_back(OSXCode::at<Code>(mDebugOverride));
			secdebug("coderep", "%p debug override to (just) %s", this, mDebugOverride.c_str());
			return;
		} catch (...) {
			secdebug("coderep", "%p debug override failed, proceeding normally", this);
		}
	}
#endif //NDEBUG
	vector<RefPointer<Code> > result;
	for (vector<string>::const_iterator it = mPaths.begin(); it != mPaths.end(); it++) {
		if (CFRef<CFArrayRef> bundles = CFBundleCreateBundlesFromDirectory(NULL,
				CFTempURL(*it, true), mSuffix.empty() ? NULL : CFStringRef(CFTempString(mSuffix)))) {
			CFIndex count = CFArrayGetCount(bundles);
			secdebug("coderep", "%p directory %s has %ld entries", this, it->c_str(), count);
			for (CFIndex n = 0; n < count; n++)
				try {
					result.push_back(new Code((CFBundleRef)CFArrayGetValueAtIndex(bundles, n)));
				} catch (...) {
					secdebug("coderep", "%p exception creating %s (skipped)",
						this, cfString(CFBundleRef(CFArrayGetValueAtIndex(bundles, n))).c_str());
				}
		} else
			secdebug("coderep", "directory %s bundle read failed", it->c_str());
	}
	secdebug("coderep", "%p total of %ld items in list", this, result.size());
	this->swap(result);
}

template void CodeRepository<GenericBundle>::update();
template void CodeRepository<ApplicationBundle>::update();
template void CodeRepository<LoadableBundle>::update();


}	// end namespace Security
