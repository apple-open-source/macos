/*
 *  Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

#ifndef _SECURITY_MANIFEST_H_
#define _SECURITY_MANIFEST_H_


#include <Security/Security.h>
#include <security_utilities/security_utilities.h>
#include <security_utilities/cfclass.h>
#include <security_cdsa_client/cspclient.h>
#include "SecManifest.h"
#include <vector>
#include <set>


// note:  The error range for the file signing library is -22040 through -22079

class ManifestItem;

class CSSMInitializer
{
protected:
	static ModuleNexus<CSSMInitializer> mInstance;

	CssmClient::Module mModule;
	CssmClient::CSP mCSP;

public:
	CSSMInitializer ();
	~CSSMInitializer ();

	static CssmClient::CSP* GetCSP ();
};



const int kSHA1DigestSize = 20;
typedef unsigned char SHA1Digest[kSHA1DigestSize];

typedef std::set<std::string> StringSet;

class ManifestItemList : private std::vector<ManifestItem*>
{
private:

friend class FileSystemItemList;

	typedef std::vector<ManifestItem*> ParentClass;

	void ConvertToStringSet (const char* path, CFArrayRef array, StringSet& stringSet);

protected:
	void DecodeURL (CFURLRef url, char *pathBuffer, CFIndex maxBufLen);
	void AddDataObject (CFDataRef data);

public:
	ManifestItemList ();
	~ManifestItemList ();
	
	void AddFileSystemObject (char* path, StringSet& exceptions, bool isRoot, bool hasAppleDoubleResourceFork);
	void AddObject (CFTypeRef object, CFArrayRef exceptionList);
	
	using ParentClass::push_back;
	using ParentClass::size;
	// using ParentClass::operator[];

	ManifestItem* operator[] (int n) {return ParentClass::operator[] (n);}
};



class FileSystemItemList : public ManifestItemList
{
public:
	void Compare (FileSystemItemList &itemList, bool compareOwnerAndGroup);
};



class RootItemList : public ManifestItemList
{
public:
	void Compare (RootItemList& itemList, bool compareOwnerAndGroup);
};



class ManifestInternal
{
protected:
	RootItemList mManifestItems;

public:
	ManifestInternal ();
	
	virtual ~ManifestInternal ();
	
	ManifestItemList& GetItemList () {return mManifestItems;}
	
	static void CompareManifests (ManifestInternal& m1,  ManifestInternal& m2, SecManifestCompareOptions options);
};



enum ManifestItemType {kManifestDataBlobItemType, kManifestFileItemType, kManifestDirectoryItemType, kManifestSymLinkItemType,
					   kManifestOtherType};

// base class for our internal object representation
class ManifestItem
{
public:
	virtual ~ManifestItem ();
	
	virtual ManifestItemType GetItemType ()  = 0;
	virtual void Compare (ManifestItem *manifestItem, bool compareOwnerAndGroup) = 0;
};



class ManifestDataBlobItem : public ManifestItem
{
protected:
	SHA1Digest mSHA1Digest;
	size_t mLength;

public:
	ManifestDataBlobItem ();
	virtual ~ManifestDataBlobItem ();
	
	ManifestItemType GetItemType ();
	
	const SHA1Digest* GetDigest ();
	void SetDigest (const SHA1Digest *sha1Digest);
	size_t GetLength ();
	void SetLength (size_t length);
	void Compare (ManifestItem* item, bool compareOwnerAndGroup);
};



class FileSystemEntryItem : public ManifestItem
{
protected:
	std::string mPath, mName;
	uid_t mUserID;
	gid_t mGroupID;
	mode_t mMode;

public:
	FileSystemEntryItem ();
	virtual ~FileSystemEntryItem ();
	
	void SetName (char* name);
	void SetPath (char* path);
	void SetUID (uid_t uid);
	void SetGID (gid_t gid);
	void SetMode (mode_t mode);
	
	const char* GetName () const;
	const std::string& GetNameAsString () const {return mName;}
	uid_t GetUID () const;
	gid_t GetGID () const;
	mode_t GetMode () const;

	void Compare (ManifestItem *manifestItem, bool compareOwnerAndGroup);
};



const int kMaxForks = 2;

class ManifestFileItem : public FileSystemEntryItem
{
protected:
	SHA1Digest mDigest[kMaxForks];
	size_t mFileLengths[kMaxForks];

	bool FileSystemHasTrueForks (char* pathToFile);
	bool HasResourceFork (char* path, std::string &pathName, struct stat &st);
	std::string ResourceFileName (char* path);
	bool FileIsMachOBinary (char* path);
	void ComputeDigestForFile (char* path, SHA1Digest &digest, size_t &length, struct stat &st);
	void ComputeDigestForAppleDoubleResourceFork (char* path, SHA1Digest &digest, size_t &length);

	int mNumForks;

public:
	ManifestFileItem ();
	virtual ~ManifestFileItem ();
	
	u_int32_t GetNumberOfForks ();
	void SetNumberOfForks (u_int32_t numForks);
	void ComputeRepresentations (struct stat &st, bool hasAppleDoubleResourceFork);
	void GetItemRepresentation (int whichFork, void* &itemRep, size_t &size);
	void SetItemRepresentation (int whichFork, const void* itemRep, size_t size);
	void SetForkLength (int whichFork, size_t length);
	size_t GetForkLength (int whichFork);

	ManifestItemType GetItemType ();
	
	void Compare (ManifestItem *manifestItem, bool compareOwnerAndGroup);
};



class ManifestDirectoryItem : public FileSystemEntryItem
{
protected:
	FileSystemItemList mDirectoryItems;

public:
	ManifestDirectoryItem ();
	virtual ~ManifestDirectoryItem ();
	
	void SetPath (char* path, StringSet &exceptions, bool isRoot);
	ManifestItemType GetItemType ();
	ManifestItemList& GetItemList () {return mDirectoryItems;}
	
	void Compare (ManifestItem *manifestItem, bool compareOwnerAndGroup);
};



class ManifestSymLinkItem : public FileSystemEntryItem
{
protected:
	std::string mContent;
	SHA1Digest mDigest;

public:
	ManifestSymLinkItem ();
	virtual ~ManifestSymLinkItem ();
	
	const SHA1Digest* GetDigest ();
	void SetDigest (const SHA1Digest* sha1Digest);
	void ComputeRepresentation ();
	ManifestItemType GetItemType ();
	
	void Compare (ManifestItem *manifestItem, bool compareOwnerAndGroup);
};



class ManifestOtherItem : public FileSystemEntryItem
{
protected:
	std::string mPath, mName;

public:
	ManifestOtherItem ();
	virtual ~ManifestOtherItem ();
	
	ManifestItemType GetItemType ();
	
	void Compare (ManifestItem *manifestItem, bool compareOwnerAndGroup);
};

#endif
