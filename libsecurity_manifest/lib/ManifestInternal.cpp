/*
 *  Copyright (c) 2004-2006 Apple Computer, Inc. All Rights Reserved.
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



#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <security_utilities/cfutilities.h>
#include <fts.h>
#include <fcntl.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <CommonCrypto/CommonDigest.h>

#include "Manifest.h"

ModuleNexus<CSSMInitializer> CSSMInitializer::mInstance;

CSSMInitializer::CSSMInitializer () : mModule (gGuidAppleCSP), mCSP (mModule)
{
}



CSSMInitializer::~CSSMInitializer ()
{
}



CssmClient::CSP* CSSMInitializer::GetCSP ()
{
	return &mInstance().mCSP;
}



//==========================  MANIFEST ITEM LIST ==========================



ManifestItemList::ManifestItemList ()
{
}



ManifestItemList::~ManifestItemList ()
{
	// throw away all of the items in the list
	iterator it = begin ();
	while (it != end ())
	{
		delete *it++;
	}
}



// return the path portion of a URL after checking to see if we support its scheme
void ManifestItemList::DecodeURL (CFURLRef url, char *pathBuffer, CFIndex maxBufLen)
{
	// get the scheme from the url and check to make sure it is a "file" scheme
	CFRef<CFStringRef> scheme (CFURLCopyScheme (url));
	if (CFStringCompare (scheme, CFSTR("file"), 0) != 0)
	{
		// we only support file URL's
		MacOSError::throwMe (errSecManifestNotSupported);
	}
	
	// convert the url into a "real" path name
	if (!CFURLGetFileSystemRepresentation (url, false, (UInt8*) pathBuffer, maxBufLen))
	{
		MacOSError::throwMe (errSecManifestNotEqual);
	}
}



void ManifestItemList::AddFileSystemObject (char* path, StringSet& exceptions, bool isRoot, bool hasAppleDoubleResourceFork)
{
	// see if our path is in the exception list.  If it is, do nothing else
	StringSet::iterator it = exceptions.find (path);
	if (it != exceptions.end ())
	{
		secdebug ("manifest", "Did not add %s to the manifest.", path);
		return;
	}
	
	// now that we have the path, do a stat and see what we have
	struct stat nodeStat;
	int result = lstat (path, &nodeStat);
	UnixError::check (result);
	
	FileSystemEntryItem* mItem;
	
	bool includeUserAndGroup = true;
	
	switch (nodeStat.st_mode & S_IFMT)
	{
		case S_IFDIR: // are we a directory?
		{
			ManifestDirectoryItem* dirItem = new ManifestDirectoryItem ();
			dirItem->SetPath (path, exceptions, isRoot);
			mItem = dirItem;
		}
		break;
		
		case S_IFREG:
		{
			ManifestFileItem* fileItem = new ManifestFileItem ();
			fileItem->SetPath (path);
			fileItem->ComputeRepresentations (nodeStat, hasAppleDoubleResourceFork);
			mItem = fileItem;
		}
		break;
		
		case S_IFLNK:
		{
			ManifestSymLinkItem* symItem = new ManifestSymLinkItem ();
			symItem->SetPath (path);
			symItem->ComputeRepresentation ();
			mItem = symItem;
			nodeStat.st_mode = S_IFLNK;
			includeUserAndGroup = false;
		}
		break;
		
		default:
		{
			ManifestOtherItem* otherItem = new ManifestOtherItem ();
			otherItem->SetPath (path);
			mItem = otherItem;
		}
		break;
	}
	
	if (includeUserAndGroup) // should we set the info?
	{
		mItem->SetUID (nodeStat.st_uid);
		mItem->SetGID (nodeStat.st_gid);
	}
	
	mItem->SetMode (nodeStat.st_mode);

	push_back (mItem);
}



void ManifestItemList::AddDataObject (CFDataRef object)
{
	// reconstruct the pointer
	SHA1Digest digest;
	CC_SHA1_CTX digestContext;
	
	CC_SHA1_Init (&digestContext);
	
	const UInt8* data = CFDataGetBytePtr (object);
	CFIndex length = CFDataGetLength (object);
	
	CC_SHA1_Update (&digestContext, data, length);
	CC_SHA1_Final (digest, &digestContext);
	
	ManifestDataBlobItem* db = new ManifestDataBlobItem ();

	db->SetDigest (&digest);
	db->SetLength (length);
	
	push_back (db);
}



void ManifestItemList::ConvertToStringSet (const char* path, CFArrayRef exceptionList, StringSet &exceptions)
{
	if (exceptionList != NULL)
	{
		std::string prefix = path;
		
		// put us in canonical form
		if (prefix[prefix.length () - 1] != '/')
		{
			prefix += '/';
		}
		
		// enumerate the list
		CFIndex max = CFArrayGetCount (exceptionList);
		CFIndex n;
		
		for (n = 0; n < max; ++n)
		{
			CFTypeRef dataRef = CFArrayGetValueAtIndex (exceptionList, n);
			if (CFGetTypeID (dataRef) != CFStringGetTypeID ())
			{
				MacOSError::throwMe (errSecManifestInvalidException);
			}
			
			// always prepend the prefix -- the spec says that all items in the exception list are relative to the root
			std::string s = prefix + cfString (CFStringRef (dataRef));
			secdebug ("manifest", "Uncanonicalized path is %s", s.c_str ());

			// canonicalize the path and insert if successful.
			char realPath [PATH_MAX];
			if (realpath (s.c_str (), realPath) != NULL)
			{
				secdebug ("manifest", "Inserted path %s as an exception", realPath);
				exceptions.insert (realPath);
			}
		}
	}
}



void ManifestItemList::AddObject (CFTypeRef object, CFArrayRef exceptionList)
{
	// get the type of the object
	CFTypeID objectID = CFGetTypeID (object);
	
	if (objectID == CFDataGetTypeID ())
	{
		AddDataObject ((CFDataRef) object);
	}
	else if (objectID == CFURLGetTypeID ())
	{
		StringSet exceptions;
		
		// get the path from the URL
		char path [PATH_MAX];
		DecodeURL ((CFURLRef) object, path, sizeof (path));

		// canonicalize
		char realPath [PATH_MAX];
		if (realpath (path, realPath) == NULL)
		{
			UnixError::throwMe ();
		}
		
		ConvertToStringSet (realPath, exceptionList, exceptions);

		AddFileSystemObject (realPath, exceptions, true, false);
	}
	else
	{
		MacOSError::throwMe (errSecManifestNotEqual);
	}
}



void RootItemList::Compare (RootItemList& item, bool compareOwnerAndGroup)
{
	// the number of items in the list has to be the same
	unsigned numItems = size ();

	if (numItems != item.size ())
	{
		MacOSError::throwMe (errSecManifestNotEqual);
	}
	
	// for a root item list, items in the manifest MUST have the same creation order
	unsigned i;
	
	for (i = 0; i < numItems; ++i)
	{
		 ManifestItem* item1 = (*this)[i];
		 ManifestItem* item2 = item[i];
		
		if (item1->GetItemType () != item2->GetItemType ())
		{
			MacOSError::throwMe (errSecManifestNotEqual);
		}
		
		item1->Compare (item2, compareOwnerAndGroup);
	}
}



class CompareManifestFileItems
{
public:
	bool operator () (ManifestItem *a, ManifestItem *b);
};



bool CompareManifestFileItems::operator () (ManifestItem *a, ManifestItem *b)
{
	FileSystemEntryItem *aa = static_cast<FileSystemEntryItem*>(a);
	FileSystemEntryItem *bb = static_cast<FileSystemEntryItem*>(b);
	
	return strcmp (aa->GetName (), bb->GetName ()) < 0;
}



void FileSystemItemList::Compare (FileSystemItemList &a, bool compareOwnerAndGroup)
{
	unsigned numItems = size ();
	
	if (numItems != a.size ())
	{
		MacOSError::throwMe (errSecManifestNotEqual);
	}

	// sort the two lists
	sort (begin (), end (), CompareManifestFileItems ());
	sort (a.begin (), a.end (), CompareManifestFileItems ());
	
	// compare each item in the list
	unsigned i;
	for (i = 0; i < numItems; ++i)
	{
		ManifestItem *thisListPtr = (*this)[i];
		ManifestItem *aListPtr = a[i];
		if (thisListPtr->GetItemType () != aListPtr->GetItemType ())
		{
			MacOSError::throwMe (errSecManifestNotEqual);
		}
		thisListPtr->Compare (aListPtr, compareOwnerAndGroup);
	}
}



//==========================  MANIFEST  ==========================



ManifestInternal::ManifestInternal ()
{
}



ManifestInternal::~ManifestInternal ()
{
	secdebug ("manifest", "Destroyed manifest internal %p", this);
}



void ManifestInternal::CompareManifests (ManifestInternal& m1,  ManifestInternal& m2, SecManifestCompareOptions options)
{
	if ((options & ~kSecManifestVerifyOwnerAndGroup) != 0)
	{
		MacOSError::throwMe (unimpErr); // we don't support these options
	}
	
	m1.mManifestItems.Compare (m2.mManifestItems, (bool) options & kSecManifestVerifyOwnerAndGroup);
}



//==========================  MANIFEST ITEM  ==========================
ManifestItem::~ManifestItem ()
{
}



//==========================  DATA BLOB ITEM  ==========================
ManifestDataBlobItem::ManifestDataBlobItem ()
{
}



ManifestDataBlobItem::~ManifestDataBlobItem ()
{
}



ManifestItemType ManifestDataBlobItem::GetItemType ()
{
	return kManifestDataBlobItemType;
}



const SHA1Digest* ManifestDataBlobItem::GetDigest ()
{
	return &mSHA1Digest;
}



void ManifestDataBlobItem::SetDigest (const SHA1Digest *sha1Digest)
{
	memcpy (&mSHA1Digest, sha1Digest, sizeof (SHA1Digest));
}



size_t ManifestDataBlobItem::GetLength ()
{
	return mLength;
}



void ManifestDataBlobItem::SetLength (size_t length)
{
	mLength = length;
}



void ManifestDataBlobItem::Compare (ManifestItem* item, bool compareOwnerAndGroup)
{
	ManifestDataBlobItem* i = static_cast<ManifestDataBlobItem*>(item);
	if (memcmp (&i->mSHA1Digest, &mSHA1Digest, sizeof (SHA1Digest)) != 0)
	{
		MacOSError::throwMe (errSecManifestNotEqual);
	}
}



//==========================  FILE SYSTEM ENTRY ITEM  ==========================



FileSystemEntryItem::FileSystemEntryItem () : mUserID (0), mGroupID (0), mMode (0)
{
}



FileSystemEntryItem::~FileSystemEntryItem ()
{
}



void FileSystemEntryItem::SetName (char* name)
{
	mName = name;
}



static char* StringTail (char* path)
{
	 char* finger = path + strlen (path) - 1;
	while (finger != path && *finger != '/')
	{
		finger -= 1;
	}
	
	if (finger != path) // did find a separator
	{
		finger += 1;
	}
	
	return finger;
}



void FileSystemEntryItem::SetPath (char* path)
{
	// save off the path
	mPath = path;
	
	// while we are at it, extract that last name of the path name and save it off as the name
	mName = StringTail (path);
	secdebug ("manifest", "Created file item for %s with name %s", mPath.c_str (), mName.c_str ());
}



void FileSystemEntryItem::SetUID (uid_t uid)
{
	mUserID = uid;
}



void FileSystemEntryItem::SetGID (gid_t gid)
{
	mGroupID = gid;
}



void FileSystemEntryItem::SetMode (mode_t mode)
{
	mMode = mode;
}



uid_t FileSystemEntryItem::GetUID () const
{
	return mUserID;
}


gid_t FileSystemEntryItem::GetGID () const
{
	return mGroupID;
}



mode_t FileSystemEntryItem::GetMode () const
{
	return mMode;
}



const char* FileSystemEntryItem::GetName () const
{
	return (char*) mName.c_str ();
}



void FileSystemEntryItem::Compare (ManifestItem *aa, bool compareOwnerAndGroup)
{
	FileSystemEntryItem* a = static_cast<FileSystemEntryItem*>(aa);
	
	if (mName != a->mName || mMode != a->mMode)
	{
		MacOSError::throwMe (errSecManifestNotEqual);
	}

	if (compareOwnerAndGroup)
	{
		if (mUserID != a->mUserID || mGroupID != a->mGroupID)
		{
			MacOSError::throwMe (errSecManifestNotEqual);
		}
	}
}



//==========================  MANIFEST FILE ITEM  ==========================



bool ManifestFileItem::FileSystemHasTrueForks (char* pathToFile)
{
    // return true if volume to which path points supports true forked files
    struct statfs st;
    int result = statfs (pathToFile, &st);
    if (result != 0)
    {
        secdebug ("manifest", "Could not get statfs (error was %s)", strerror (errno));
        UnixError::throwMe ();
    }
    
    return strcmp (st.f_fstypename, "afpfs") == 0 || strcmp (st.f_fstypename, "hfs") == 0;
}



std::string ManifestFileItem::ResourceFileName (char* path)
{
    std::string filePath;

    if (FileSystemHasTrueForks (path))
    {
        filePath = path;
		
		return filePath + "/rsrc";
    }
    else
    {
		return "";
    }
    
    return filePath;
}



bool ManifestFileItem::HasResourceFork (char* pathToFile, std::string &result, struct stat &st)
{
    // try to get the stat on the file.  If it works, the file exists
	result = ResourceFileName (pathToFile);
	if (result.length () != 0)
	{
		int stresult = lstat (result.c_str (), &st);
		if (stresult == 0)
		{
			return st.st_size != 0;
		}
	}
	
    return false;
}



ManifestFileItem::ManifestFileItem () : mNumForks (1)
{
}



ManifestFileItem::~ManifestFileItem ()
{
	secdebug ("manifest", "Destroyed manifest item %p for path %s", this, mPath.c_str ());
}



ManifestItemType ManifestFileItem::GetItemType () 
{
	return kManifestFileItemType;
}



u_int32_t ManifestFileItem::GetNumberOfForks () 
{
	return mNumForks;
}



void ManifestFileItem::SetNumberOfForks (u_int32_t numForks)
{
	mNumForks = numForks;
}



bool ManifestFileItem::FileIsMachOBinary (char* path)
{
	return false;
}



void ManifestFileItem::SetForkLength (int which, size_t length)
{
	mFileLengths[which] = length;
}



size_t ManifestFileItem::GetForkLength (int which)
{
	return mFileLengths[which];
}



void ManifestFileItem::ComputeRepresentations (struct stat &st, bool hasAppleDoubleResourceFork)
{
	// digest the data fork
	mNumForks = 1;
	ComputeDigestForFile ((char*) mPath.c_str (), mDigest[0], mFileLengths[0], st);
	
	struct stat stat2;
	std::string resourceForkName;
	if (hasAppleDoubleResourceFork)
	{
		mNumForks = 2;
		
		resourceForkName = mPath;
		// walk back to find the beginning of the path and insert ._
		int i = resourceForkName.length () - 1;
		while (i >= 0 && resourceForkName[i] != '/')
		{
			i -= 1;
		}
		
		i += 1;
		
		resourceForkName.insert (i, "._");
		
		ComputeDigestForAppleDoubleResourceFork ((char*) resourceForkName.c_str(), mDigest[1], mFileLengths[1]);
	}
	else if (HasResourceFork ((char*) mPath.c_str (), resourceForkName, stat2))
	{
		mNumForks = 2;
		ComputeDigestForFile ((char*) resourceForkName.c_str (), mDigest[1], mFileLengths[1], stat2);
	}
}



static const int kReadChunkSize = 4096 * 4;



static u_int32_t ExtractUInt32 (u_int8_t *&finger)
{
	u_int32_t result = 0;
	int i;
	for (i = 0; i < 4; ++i)
	{
		result = (result << 8) | *finger++;
	}
	
	return result;
}



void ManifestFileItem::ComputeDigestForAppleDoubleResourceFork (char* name, SHA1Digest &digest, size_t &fileLength)
{
	secdebug ("manifest", "Creating digest for AppleDouble resource fork %s", name);

	CC_SHA1_CTX digestContext;
	CC_SHA1_Init (&digestContext);
	
	// bring the file into memory
	int fileNo = open (name, O_RDONLY, 0);
	if (fileNo == -1)
	{
		UnixError::throwMe ();
	}
	
	// figure out how big the file is.
	struct stat st;
	int result = fstat (fileNo, &st);
	if (result == -1)
	{
		UnixError::throwMe ();
	}
	
	u_int8_t *buffer = new u_int8_t[st.st_size];
	ssize_t bytesRead = read (fileNo, buffer, st.st_size);
	close (fileNo);
	
	if (bytesRead != st.st_size)
	{
		delete buffer;
		UnixError::throwMe ();
	}
	
	// walk the entry table to find the offset to our resource fork
	u_int8_t *bufPtr = buffer + 24; // size of the header + filler
	
	// compute the number of entries in the file
	int numEntries = (((int) bufPtr[0]) << 8) | (int) (bufPtr [1]);
	bufPtr += 2;
	
	ssize_t length = 0;
	ssize_t offset = 0;
	
	int i;
	for (i = 0; i < numEntries; ++i)
	{
		// bufPtr points to an entry descriptor.  Four bytes for the ID, four for the offset, four for the length
		ssize_t id = ExtractUInt32 (bufPtr);
		offset = ExtractUInt32 (bufPtr);
		length = ExtractUInt32 (bufPtr);
		
		if (id == 2) // is it the resource fork?
		{
			break;
		}
	}
	
	if (i >= numEntries) // did we run off the end?  This had better not happen
	{
		MacOSError::throwMe (errSecManifestNotSupported);
	}
	
	fileLength = length;

	// digest the data
	CC_SHA1_Update (&digestContext, buffer + offset, length);
	
	// compute the SHA1 hash
	CC_SHA1_Final (digest, &digestContext);
	
	delete buffer;
}



void ManifestFileItem::ComputeDigestForFile (char* name, SHA1Digest &digest, size_t &fileLength, struct stat &st)
{
	secdebug ("manifest", "Creating digest for %s", name);

	// create a context for the digest operation
	CC_SHA1_CTX digestContext;
	CC_SHA1_Init (&digestContext);

	
	int fileNo = open (name, O_RDONLY, 0);
	if (fileNo == -1)
	{
		UnixError::throwMe ();
	}
	
	fileLength = st.st_size;

	if (st.st_size != 0)
	{
		// read the file
		char buffer[kReadChunkSize];
		
		ssize_t bytesRead;
		while ((bytesRead = read (fileNo, buffer, kReadChunkSize)) != 0)
		{
			// digest the read data
			CC_SHA1_Update (&digestContext, buffer, bytesRead);
		}
		
		// compute the SHA1 hash
		CC_SHA1_Final (digest, &digestContext);
	}

	close (fileNo);
}



void ManifestFileItem::GetItemRepresentation (int whichFork, void* &itemRep, size_t &size) 
{
	itemRep = (void*) &mDigest[whichFork];
	size = kSHA1DigestSize;
}



void ManifestFileItem::SetItemRepresentation (int whichFork, const void* itemRep, size_t size) 
{
	memcpy ((void*) &mDigest[whichFork], itemRep, size);
}



void ManifestFileItem::Compare (ManifestItem *manifestItem, bool compareOwnerAndGroup)
{
	FileSystemEntryItem::Compare (manifestItem, compareOwnerAndGroup);

	ManifestFileItem* item = static_cast< ManifestFileItem*>(manifestItem);
	
	secdebug ("manifest", "Comparing file item %s against %s", GetName (), item->GetName ());

	// the number of forks should be equal
	if (mNumForks != item->mNumForks)
	{
		MacOSError::throwMe (errSecManifestNotEqual);
	}
	
	// compare file lengths
	int i;
	for (i = 0; i < mNumForks; ++i)
	{
		if (mFileLengths[i] != item->mFileLengths[i])
		{
			MacOSError::throwMe (errSecManifestNotEqual);
		}

		if (memcmp (&mDigest[i], item->mDigest[i], kSHA1DigestSize) != 0)
		{
			MacOSError::throwMe (errSecManifestNotEqual);
		}
	}
}



//==========================  MANIFEST DIRECTORY ITEM  ==========================



ManifestDirectoryItem::ManifestDirectoryItem ()
{
}



ManifestDirectoryItem::~ManifestDirectoryItem ()
{
	secdebug ("manifest", "Destroyed directory item %p for path %s", this, mPath.c_str ());
}


const char* kAppleDoublePrefix = "._";
const int kAppleDoublePrefixLength = 2;

static int CompareFilenames (const FTSENT** a, const FTSENT** b)
{
	// ._name is always greater than name
	// otherwise, ._ is ignored for sorting purposes
	const char* aa = (*a)->fts_name;
	const char* bb = (*b)->fts_name;
	bool aHasPrefix = false;
	
	if (strncmp (aa, kAppleDoublePrefix, kAppleDoublePrefixLength) == 0) // do we have an appledouble prefix?
	{
		aHasPrefix = true;
		aa += kAppleDoublePrefixLength;
	}
	
	if (strncmp (bb, kAppleDoublePrefix, kAppleDoublePrefixLength) == 0) // do we have an appledouble prefix?
	{
		bb += kAppleDoublePrefixLength;
	}
	
	int compare = strcmp (aa, bb);
	
	if (compare == 0 && aHasPrefix)
	{
		return 1;
	}
		
	return compare;
}



const u_int8_t kAppleDoubleMagicNumber[] = {0x00, 0x05, 0x16, 0x07};



static bool PathIsAppleDoubleFile (const char* path)
{
	// Open the file and check the "magic number".
	int fRef = open (path, O_RDONLY, 0);
	
	u_int8_t buffer[4];
	
	// read the first four bytes of the file
	ssize_t bytesRead = read(fRef, buffer, 4);
	if (bytesRead == -1)
	{
		int err = errno;
		close (fRef);
		UnixError::throwMe (err);
	}
	
	close (fRef);
	
	if (bytesRead != 4) // did we get enough bytes?
	{
		return false;
	}
	
	// what we got had better be the proper magic number for this file type
	int i;
	for (i = 0; i < 4; ++i)
	{
		if (buffer[i] != kAppleDoubleMagicNumber[i])
		{
			return false;
		}
	}
	
	return true;
}



void ManifestDirectoryItem::SetPath (char* path, StringSet &exceptions, bool isRoot)
{
	if (isRoot)
	{
		mName = "/";
		mPath = path;
	}
	else
	{
		FileSystemEntryItem::SetPath (path);
	}
	
	secdebug ("manifest", "Added directory entry for %s with name %s", mPath.c_str (), mName.c_str ());
	
	// enumerate the contents of the directory.
	char* path_argv[] = { path, NULL };
	FTS* thisDir = fts_open (path_argv, FTS_PHYSICAL | FTS_NOCHDIR | FTS_NOSTAT | FTS_XDEV, CompareFilenames);
	if (thisDir == NULL) // huh?  The file disappeared or isn't a directory any more
	{
		UnixError::throwMe ();
	}
	
	(void)fts_read(thisDir);
	FTSENT* dirEnt = fts_children (thisDir, FTS_NAMEONLY);
	
	while (dirEnt != NULL)
	{
		// get the next entry
		FTSENT* dirEntNext = dirEnt->fts_link;
		bool hasAppleDoubleResourceFork = false;

		// see if it is an AppleDouble resource fork for this file
		if (dirEntNext &&
			strncmp (dirEntNext->fts_name, kAppleDoublePrefix, kAppleDoublePrefixLength) == 0 &&
			strcmp (dirEnt->fts_name, dirEntNext->fts_name + kAppleDoublePrefixLength) == 0)
		{
			if (PathIsAppleDoubleFile ((mPath + "/" + dirEntNext->fts_name).c_str ()))
			{
				hasAppleDoubleResourceFork = true;
				dirEntNext = dirEntNext->fts_link;
			}
		}
		
		// figure out what this is pointing to.
		std::string fileName = mPath + "/" + dirEnt->fts_name;

		mDirectoryItems.AddFileSystemObject ((char*) fileName.c_str(), exceptions, false, hasAppleDoubleResourceFork);
		
		dirEnt = dirEntNext;
	}
	
	fts_close(thisDir);
}



ManifestItemType ManifestDirectoryItem::GetItemType () 
{
	return kManifestDirectoryItemType;
}



void ManifestDirectoryItem::Compare (ManifestItem* a, bool compareOwnerAndGroup)
{
	FileSystemEntryItem::Compare (a, compareOwnerAndGroup);
	ManifestDirectoryItem* aa = static_cast<ManifestDirectoryItem*>(a);
	secdebug ("manifest", "Comparing directory item %s against %s", GetName (), aa->GetName ());
	mDirectoryItems.Compare (aa->mDirectoryItems, compareOwnerAndGroup);
}



//==========================  MANIFEST SYMLINK ITEM  ==========================



ManifestSymLinkItem::ManifestSymLinkItem ()
{
}



ManifestSymLinkItem::~ManifestSymLinkItem ()
{
	secdebug ("manifest", "Destroyed symlink item for %s", mPath.c_str ());
}



void ManifestSymLinkItem::ComputeRepresentation ()
{
	char path [FILENAME_MAX];
	int result = readlink (mPath.c_str (), path, sizeof (path));
	secdebug ("manifest", "Read content %s for %s", path, mPath.c_str ());
	
	// create a digest context
	CC_SHA1_CTX digestContext;
	CC_SHA1_Init (&digestContext);
	
	// digest the data
	CC_SHA1_Update (&digestContext, path, result);

	// compute the result
	CC_SHA1_Final (mDigest, &digestContext);
	
	UnixError::check (result);
}



const SHA1Digest* ManifestSymLinkItem::GetDigest () 
{
	return &mDigest;
}



void ManifestSymLinkItem::SetDigest (const SHA1Digest* digest)
{
	memcpy (mDigest, digest, sizeof (SHA1Digest));
}



ManifestItemType ManifestSymLinkItem::GetItemType () 
{
	return kManifestSymLinkItemType;
}



void ManifestSymLinkItem::Compare (ManifestItem *a, bool compareOwnerAndGroup)
{
	FileSystemEntryItem::Compare (a, compareOwnerAndGroup);
	ManifestSymLinkItem* aa = static_cast<ManifestSymLinkItem*>(a);
	secdebug ("manifest", "Comparing symlink item %s against %s", GetName (), aa->GetName ());
	
	// now compare the data
	if (memcmp (&mDigest, &aa->mDigest, kSHA1DigestSize) != 0)
	{
		MacOSError::throwMe (errSecManifestNotEqual);
	}
}



//==========================  MANIFEST OTHER ITEM  ==========================



ManifestOtherItem::ManifestOtherItem ()
{
}



ManifestOtherItem::~ManifestOtherItem ()
{
	secdebug ("manifest", "Destroyed other item for path %s", mPath.c_str ());
}



ManifestItemType ManifestOtherItem::GetItemType () 
{
	return kManifestOtherType;
}



void ManifestOtherItem::Compare (ManifestItem *a, bool compareOwnerAndGroup)
{
	FileSystemEntryItem::Compare (a, compareOwnerAndGroup);
	secdebug ("manifest", "Comparing other item %s against %s", GetName (), static_cast<FileSystemEntryItem*>(a)->GetName ());
}
