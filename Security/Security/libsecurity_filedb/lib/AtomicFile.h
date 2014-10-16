/*
 * Copyright (c) 2000-2001,2003,2011-2012,2014 Apple Inc. All Rights Reserved.
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
//  AtomicFile.h - Description t.b.d.
//
#ifndef _SECURITY_ATOMICFILE_H_
#define _SECURITY_ATOMICFILE_H_  1

#include <security_utilities/refcount.h>
#include <Security/cssm.h>
#include <string>
#include <sys/stat.h>

namespace Security
{

class AtomicBufferedFile;
class AtomicLockedFile;
class AtomicTempFile;

class AtomicFile
{
public:
	AtomicFile(const std::string &inPath);
	~AtomicFile();

    // Aquire the write lock and remove the file.
    void performDelete();

    // Aquire the write lock and rename the file.
    void rename(const std::string &inNewPath);

	// Lock the file for writing and return a newly created AtomicTempFile.
    RefPointer<AtomicTempFile> create(mode_t mode);

    // Lock the file for writing and return a newly created AtomicTempFile.
	RefPointer<AtomicTempFile> write();

	// Return a bufferedFile containing current version of the file for reading.
	RefPointer<AtomicBufferedFile> read();

	const string& path() const { return mPath; }
	const string& dir() const { return mDir; }
	const string& file() const { return mFile; }
	const string& lockFileName() { return mLockFilePath; }

	mode_t mode() const;
	bool isOnLocalFileSystem() {return mIsLocalFileSystem;}

    enum OffsetType
	{
        FromStart,
		FromEnd			// only works with offset of 0
    };

	static void pathSplit(const std::string &inFull, std::string &outDir, std::string &outFile);
	static void mkpath(const std::string &inDir, mode_t mode = 0777);
	static int ropen(const char *const name, int flags, mode_t mode);
	static int rclose(int fd);

private:
	bool mIsLocalFileSystem;
	string mPath;
	string mDir;
	string mFile;
	string mLockFilePath;
};


//
// AtomicBufferedFile - This represents an instance of a file opened for reading.
// The file is read into memory and closed after this is done.
// The memory is released when this object is destroyed.
//
class AtomicBufferedFile : public RefCount
{
public:
	AtomicBufferedFile(const std::string &inPath, bool isLocalFileSystem);
	~AtomicBufferedFile();

	// Open the file and return it's size.
	off_t open();

	// Read inLength bytes starting at inOffset.
	const uint8 *read(off_t inOffset, off_t inLength, off_t &outLength);

	// Return the current mode bits of the file
	mode_t mode();

	// Close the file (this doesn't release the buffer).
	void close();

	// Return the length of the file.
	off_t length() const { return mLength; }

private:
	void loadBuffer();
	void unloadBuffer();
	
private:
	// Complete path to the file
	string mPath;

	// File descriptor to the file or -1 if it's not currently open.
	int mFileRef;

	// This is where the data from the file is read in to.
	uint8 *mBuffer;

	// Length of file in bytes.
	off_t mLength;
	
	// Is on a local file system
	bool mIsMapped;
};


//
// AtomicTempFile - A temporary file to write changes to.
//
class AtomicTempFile : public RefCount
{
public:
	// Start a write for a new file.
	AtomicTempFile(AtomicFile &inFile, const RefPointer<AtomicLockedFile> &inLockedFile, mode_t mode);

	// Start a write of an existing file.
	AtomicTempFile(AtomicFile &inFile, const RefPointer<AtomicLockedFile> &inLockedFile);

	~AtomicTempFile();

    // Commit the current create or write and close the write file.
    void commit();

    void write(AtomicFile::OffsetType inOffsetType, off_t inOffset, const uint32 *inData, uint32 inCount);
    void write(AtomicFile::OffsetType inOffsetType, off_t inOffset, const uint8 *inData, size_t inLength);
    void write(AtomicFile::OffsetType inOffsetType, off_t inOffset, const uint32 inData);

private:
	// Called by both constructors.
	void create(mode_t mode);

	// Fsync the file
	void fsync();

	// Close the file
	void close();

    // Rollback the current create or write (happens automatically if commit() isn't called before the destructor is).
    void rollback() throw();

private:
	// Our AtomicFile object.
	AtomicFile &mFile;

	RefPointer<AtomicLockedFile> mLockedFile;

	// Complete path to the file
	string mPath;

	// File descriptor to the file or -1 if it's not currently open.
	int mFileRef;

	// If this is true we unlink both mPath and mFile.path() when we rollback.
	bool mCreating;
};


class FileLocker
{
public:
	virtual ~FileLocker();
	
	virtual void lock(mode_t mode) = 0;
	virtual void unlock() = 0;
};



class LocalFileLocker : public FileLocker
{
public:
	LocalFileLocker(AtomicFile &inFile);
	virtual ~LocalFileLocker();
	
	virtual void lock(mode_t mode);
	virtual void unlock();

private:
	int mLockFile;
	string mPath;
};



class NetworkFileLocker : public FileLocker
{
public:
	NetworkFileLocker(AtomicFile &inFile);
	virtual ~NetworkFileLocker();
	
	virtual void lock(mode_t mode);
	virtual void unlock();

private:
	std::string unique(mode_t mode);
	int rlink(const char *const old, const char *const newn, struct stat &sto);
	int myrename(const char *const old, const char *const newn);
	int xcreat(const char *const name, mode_t mode, time_t &tim);

	// The directory in which we create the lock
	string mDir;

	// Complete path to the file
	string mPath;
};



// The current lock being held.
class AtomicLockedFile : public RefCount
{
public:
	// Create a write lock for inFile.
	AtomicLockedFile(AtomicFile &inFile);

	~AtomicLockedFile();

private:
	void lock(mode_t mode = (S_IRUSR|S_IRGRP|S_IROTH) /* === 0444 */);
	void unlock() throw();

private:
	FileLocker* mFileLocker;
};


} // end namespace Security


#endif // _SECURITY_ATOMICFILE_H_
