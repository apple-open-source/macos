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
// osxsigning - MacOS X's standard signable objects.
//
#ifndef _OSXSIGNING
#define _OSXSIGNING

#include <Security/codesigning.h>
#include <Security/refcount.h>
#include <Security/cspclient.h>
#include <limits.h>
#include <string>
#include <CoreFoundation/CFBundle.h>

#ifdef _CPP_OSXSIGNING
#pragma export on
#endif


namespace Security {
namespace CodeSigning {


//
// A Signable with OS X support calls added
//
class OSXCode : public RefCount, public Signable {
public:
	// encoding and decoding as a UTF-8 string
	virtual string encode() const = 0;
	static OSXCode *decode(const char *extForm);
	
public:
	// creating OSXCode objects
	static OSXCode *main();
	static OSXCode *at(const char *path);
	
public:
	// produce the best approximation of a path that, when handed to at(),
	// will yield an OSXCode that's the most like this one
	virtual string canonicalPath() const = 0;

protected:
	OSXCode() { }	// nonpublic
	static void scanFile(const char *pathname, Signer::State &state);	// scan an entire file
	static string getPath(CFURLRef url);
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
	
protected:
	void scanContents(Signer::State &state) const;

private:
	string mPath;			// UTF8 pathname to executable
};


//
// A generic bundle
//
class GenericBundle : public OSXCode {
public:
	GenericBundle(const char *path);
	~GenericBundle();

	string encode() const;
	
	string canonicalPath() const;
	string path() const				{ return mPath; }
	string executablePath() const	{ return getPath(CFBundleCopyExecutableURL(mBundle)); }
	
	virtual void *lookupSymbol(const char *name);
	
protected:
	void scanContents(Signer::State &state) const;
	
protected:
	string mPath;			// UTF8 path to bundle directory
	CFBundleRef mBundle;	// CF-style bundle object
};

class ApplicationBundle : public GenericBundle {
public:
	ApplicationBundle(const char *pathname) : GenericBundle(pathname) { }
};

class LoadableBundle : public GenericBundle {
public:
	LoadableBundle(const char *pathname) : GenericBundle(pathname) { }
	
	virtual bool isLoaded() const;
	virtual void load();
	virtual void unload();
};


} // end namespace CodeSigning

} // end namespace Security

#ifdef _CPP_OSXSIGNING
#pragma export off
#endif


#endif //_OSXSIGNING
