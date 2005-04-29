/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// osxcode - MacOS X's standard code objects
//
#ifndef _H_OSXCODE
#define _H_OSXCODE

#include <security_utilities/codesigning.h>
#include <security_utilities/refcount.h>
#include <security_utilities/cfutilities.h>
#include <limits.h>
#include <string>
#include <vector>
#include <CoreFoundation/CFBundle.h>


namespace Security {


//
// A Signable with OS X support calls added
//
class OSXCode : public RefCount, public CodeSigning::Signable {
public:
	// encoding and decoding as a UTF-8 string
	virtual string encode() const = 0;
	static OSXCode *decode(const char *extForm);
	
public:
	// creating OSXCode objects
	static RefPointer<OSXCode> main();
	static RefPointer<OSXCode> at(const char *path);
	static RefPointer<OSXCode> at(const std::string &path)
	{ return at(path.c_str()); }
	
	template <class Code>
	static RefPointer<Code> main() { return restrict<Code>(main()); }
	template <class Code>
	static RefPointer<Code> at(const char *path) { return restrict<Code>(at(path)); }
	template <class Code>
	static RefPointer<Code> at(const std::string &path)	{ return restrict<Code>(at(path)); }

	// restrict (validate) to subclass or throw
	template <class Sub>
	static RefPointer<Sub> restrict(RefPointer<OSXCode> in)
	{
		if (Sub *sub = dynamic_cast<Sub *>(in.get()))
			return sub;
		else
			UnixError::throwMe(ENOEXEC);
	}
	
public:
	// produce the best approximation of a path that, when handed to at(),
	// will yield an OSXCode that's the most like this one
	virtual string canonicalPath() const = 0;
	virtual string executablePath() const = 0;

protected:
	OSXCode() { }	// nonpublic
	static void scanFile(const char *pathname, CodeSigning::Signer::State &state);	// scan an entire file
};


//
// A simple executable tool.
//
class ExecutableTool : public OSXCode {
public:
	ExecutableTool(const char *path) : mPath(path) { }
	string encode() const;
	
	string path() const		{ return mPath; }
	string canonicalPath() const;
	string executablePath() const;
	
protected:
	void scanContents(CodeSigning::Signer::State &state) const;

private:
	string mPath;			// UTF8 pathname to executable
};


//
// A generic bundle
//
class GenericBundle : public OSXCode {
public:
	GenericBundle(const char *path, const char *execPath = NULL);	// from root and optional exec path
	GenericBundle(CFBundleRef bundle, const char *root = NULL);		// from existing CFBundleRef
	~GenericBundle();

	string encode() const;
	
	string canonicalPath() const;
	string path() const				{ return mPath; }
	string executablePath() const;
	string identifier() const		{ return cfString(CFBundleGetIdentifier(cfBundle())); }
	CFTypeRef infoPlistItem(const char *name) const;	// not retained

	string resourcePath() const		{ return cfString(CFBundleCopyResourcesDirectoryURL(cfBundle()), true); }
	string resource(const char *name, const char *type, const char *subdir = NULL);
	void resources(vector<string> &paths, const char *type, const char *subdir = NULL);
	
	virtual void *lookupSymbol(const char *name);

protected:
	void scanContents(CodeSigning::Signer::State &state) const;
	CFBundleRef cfBundle() const;
	
protected:
	string mPath;			// UTF8 path to bundle directory
	mutable string mExecutablePath;	// cached or determined path to main executable
	mutable CFBundleRef mBundle; // CF-style bundle object (lazily built)
};

class ApplicationBundle : public GenericBundle {
public:
	ApplicationBundle(const char *pathname) : GenericBundle(pathname) { }
	ApplicationBundle(CFBundleRef bundle) : GenericBundle(bundle) { }
};

class LoadableBundle : public GenericBundle {
public:
	LoadableBundle(const char *pathname) : GenericBundle(pathname) { }
	LoadableBundle(CFBundleRef bundle) : GenericBundle(bundle) { }
	
	virtual bool isLoaded() const;
	virtual void load();
	virtual void unload();
};


} // end namespace Security


#endif //_H_OSXCODE
