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
#ifndef _H_TOKENCACHE
#define _H_TOKENCACHE

#include <security_utilities/refcount.h>
#include <Security/cssm.h>


//
// A little helper
//
class Rooted {
public:
	Rooted() { }
	Rooted(const char *root) : mRoot(root) { }
	Rooted(const string &root) : mRoot(root) { }
	
	string root() const { return mRoot; }
	string path(const char *sub) const;
	string path(const string &sub) const { return path(sub.c_str()); }

protected:
	void root(const string &s);

private:
	string mRoot;				// root of this tree
};


//
// An on-disk cache area.
// You'll only want a single one, though nothing keeps you from
// making multiples if you like.
//
class TokenCache : public Rooted {
public:
	TokenCache(const char *root);
	~TokenCache();
	
	uid_t tokendUid() const { return mTokendUid; }
	gid_t tokendGid() const { return mTokendGid; }
	
public:
	class Token : public RefCount, public Rooted {
	public:
		friend class TokenCache;
		Token(TokenCache &cache, const std::string &uid);
		Token(TokenCache &cache);
		~Token();
		
		enum Type { existing, created, temporary };
		Type type() const { return mType; }

		TokenCache &cache;
		uint32 subservice() const { return mSubservice; }
		string workPath() const;
		string cachePath() const;
		
		string printName() const;
		void printName(const string &name);
		
		uid_t tokendUid() const { return cache.tokendUid(); }
		gid_t tokendGid() const { return cache.tokendGid(); }
	
	protected:		
		void init(Type type);

	private:
		uint32 mSubservice;		// subservice id assigned
		Type mType;				// type of Token cache entry
	};

public:
	uint32 allocateSubservice();

private:
	enum Owner { securityd, tokend };
	void makedir(const char *path, int flags, mode_t mode, Owner owner);
	void makedir(const string &path, int flags, mode_t mode, Owner owner)
	{ return makedir(path.c_str(), flags, mode, owner); }
	
private:
	uint32 mLastSubservice; // last subservice id issued

	uid_t mTokendUid;		// uid of daemons accessing this token cache
	gid_t mTokendGid;		// gid of daemons accessing this token cache
};


#endif //_H_TOKENCACHE
