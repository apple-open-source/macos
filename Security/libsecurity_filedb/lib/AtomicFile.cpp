/*
 * Copyright (c) 2000-2012 Apple Inc. All Rights Reserved.
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


#include <security_filedb/AtomicFile.h>

#include <security_utilities/devrandom.h>
#include <CommonCrypto/CommonDigest.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/cssm.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <syslog.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <copyfile.h>
#include <sandbox.h>
#include <set>

#define kAtomicFileMaxBlockSize INT_MAX


//
//  AtomicFile.cpp
//
AtomicFile::AtomicFile(const std::string &inPath) :
	mPath(inPath)
{
	pathSplit(inPath, mDir, mFile);
	
    if (mDir.length() == 0)
    {
        const char* buffer = getwd(NULL);
        mDir = buffer;
        free((void*) buffer);
    }
    
    mDir += '/';

	// determine if the path is on a local or a networked volume
	struct statfs info;
	int result = statfs(mDir.c_str(), &info);
	if (result == -1) // error on opening?
	{
		mIsLocalFileSystem = false; // revert to the old ways if we can't tell what kind of system we have
	}
	else
	{
		mIsLocalFileSystem = (info.f_flags & MNT_LOCAL) != 0;
		if (mIsLocalFileSystem)
		{
			// compute the name of the lock file for this file
			CC_SHA1_CTX ctx;
			CC_SHA1_Init(&ctx);
			CC_SHA1_Update(&ctx, (const void*) mFile.c_str(), (CC_LONG)mFile.length());
			u_int8_t digest[CC_SHA1_DIGEST_LENGTH];
			CC_SHA1_Final(digest, &ctx);

			u_int32_t hash = (digest[0] << 24) | (digest[1] << 16) | (digest[2] << 8) | digest[3];
			
			char buffer[256];
			sprintf(buffer, "%08X", hash);
			mLockFilePath = mDir + ".fl" + buffer;
		}
	}
}

AtomicFile::~AtomicFile()
{
}

// Aquire the write lock and remove the file.
void
AtomicFile::performDelete()
{
	AtomicLockedFile lock(*this);
	if (::unlink(mPath.c_str()) != 0)
	{
		int error = errno;
		secdebug("atomicfile", "unlink %s: %s", mPath.c_str(), strerror(error));
        if (error == ENOENT)
			CssmError::throwMe(CSSMERR_DL_DATASTORE_DOESNOT_EXIST);
		else
			UnixError::throwMe(error);
	}

	// unlink our lock file
	::unlink(mLockFilePath.c_str());
}

// Aquire the write lock and rename the file (and bump the version and stuff).
void
AtomicFile::rename(const std::string &inNewPath)
{
	const char *path = mPath.c_str();
	const char *newPath = inNewPath.c_str();

	// @@@ lock the destination file too.
	AtomicLockedFile lock(*this);
	if (::rename(path, newPath) != 0)
	{
		int error = errno;
		secdebug("atomicfile", "rename(%s, %s): %s", path, newPath, strerror(error));
		UnixError::throwMe(error);
	}
}

// Lock the file for writing and return a newly created AtomicTempFile.
RefPointer<AtomicTempFile>
AtomicFile::create(mode_t mode)
{
	const char *path = mPath.c_str();

	// First make sure the directory to this file exists and is writable
	mkpath(mDir);

	RefPointer<AtomicLockedFile> lock(new AtomicLockedFile(*this));
	int fileRef = ropen(path, O_WRONLY|O_CREAT|O_EXCL, mode);
    if (fileRef == -1)
    {
        int error = errno;
		secdebug("atomicfile", "open %s: %s", path, strerror(error));

        // Do the obvious error code translations here.
		// @@@ Consider moving these up a level.
        if (error == EACCES)
			CssmError::throwMe(CSSM_ERRCODE_OS_ACCESS_DENIED);
        else if (error == EEXIST)
			CssmError::throwMe(CSSMERR_DL_DATASTORE_ALREADY_EXISTS);
		else
			UnixError::throwMe(error);
    }
	rclose(fileRef);

	try
	{
		// Now that we have created the lock and the new db file create a tempfile
		// object.
		RefPointer<AtomicTempFile> temp(new AtomicTempFile(*this, lock, mode));
		secdebug("atomicfile", "%p created %s", this, path);
		return temp;
	}
	catch (...)
	{
		// Creating the temp file failed so remove the db file we just created too.
		if (::unlink(path) == -1)
		{
			secdebug("atomicfile", "unlink %s: %s", path, strerror(errno));
		}
		throw;
	}
}

// Lock the database file for writing and return a newly created AtomicTempFile.
// If the parent directory allows the write we're going to allow this.  Previous
// versions checked for writability of the db file and that caused problems when
// setuid programs had made entries.  As long as the db (keychain) file is readable
// this function can make the newer keychain file with the correct owner just by virtue
// of the copy that takes place.

RefPointer<AtomicTempFile>
AtomicFile::write()
{

	RefPointer<AtomicLockedFile> lock(new AtomicLockedFile(*this));
	return new AtomicTempFile(*this, lock);
}

// Return a bufferedFile containing current version of the file for reading.
RefPointer<AtomicBufferedFile>
AtomicFile::read()
{
	return new AtomicBufferedFile(mPath, mIsLocalFileSystem);
}

mode_t
AtomicFile::mode() const
{
	const char *path = mPath.c_str();
	struct stat st;
	if (::stat(path, &st) == -1)
	{
		int error = errno;
		secdebug("atomicfile", "stat %s: %s", path, strerror(error));
		UnixError::throwMe(error);
	}
	return st.st_mode;
}

// Split full into a dir and file component.
void
AtomicFile::pathSplit(const std::string &inFull, std::string &outDir, std::string &outFile)
{
	std::string::size_type slash, len = inFull.size();
	slash = inFull.rfind('/');
	if (slash == std::string::npos)
	{
		outDir = "";
		outFile = inFull;
	}
	else if (slash + 1 == len)
	{
		outDir = inFull;
		outFile = "";
	}
	else
	{
		outDir = inFull.substr(0, slash + 1);
		outFile = inFull.substr(slash + 1, len);
	}
}

//
// Make sure the directory up to inDir exists inDir *must* end in a slash.
//
void
AtomicFile::mkpath(const std::string &inDir, mode_t mode)
{
    // see if the file already exists and is a directory
    struct stat st;
    int result = stat(inDir.c_str(), &st);
    
    if (result == 0) // file exists
    {
        if ((st.st_mode & S_IFDIR) == 0)
        {
            // whatever was there, it wasn't a directory.  That's really bad, so complain
            syslog(LOG_ALERT, "Needed a directory at %s, but the file that was there was not one.\n", inDir.c_str());
            UnixError::throwMe(ENOTDIR);
        }
    }
    else
    {
        // the file did not exist, try to create it
        result = mkpath_np(inDir.c_str(), 0777); // make the directory with umask
        if (result != 0)
        {
            // mkpath_np does not set errno, you have to look at the result.
            UnixError::throwMe(result);
        }
    }
    
    // Double check and see if we got what we hoped for
    result = stat(inDir.c_str(), &st);
    if (result != 0)
    {
        UnixError::throwMe(errno);
    }
    
    if ((st.st_mode & S_IFDIR) == 0)
    {
        // we didn't create a dictionary?  That's curious...
        syslog(LOG_ALERT, "Failed to create a directory when we asked for one to be created at %s\n", inDir.c_str());
        UnixError::throwMe(ENOTDIR);
    }
}

int
AtomicFile::ropen(const char *const name, int flags, mode_t mode)
{
    bool isCreate = (flags & O_CREAT) != 0;
    
    /*
        The purpose of checkForRead and checkForWrite is to mitigate
        spamming of the log when a user has installed certain third
        party software packages which create additional keychains.
        Certain applications use a custom sandbox profile which do not
        permit this and so the user gets a ton of spam in the log.
        This turns into a serious performance problem.
        
        We handle this situation by checking two factors:
        
            1:  If the user is trying to create a file, we send the
                request directly to open.  This is the right thing
                to do, as we don't want most applications creating
                keychains unless they have been expressly authorized
                to do so.
                
                The layers above this one only set O_CREAT when a file
                doesn't exist, so the case where O_CREAT can be called
                on an existing file is irrelevant.
            
            2:  If the user is trying to open the file for reading or
                writing, we check with the sandbox mechanism to see if
                the operation will be permitted (and tell it not to
                log if it the operation will fail).
                
                If the operation is not permitted, we return -1 which
                emulates the behavior of open.  sandbox_check sets
                errno properly, so the layers which call this function
                will be able to act as though open had been called.
    */

    bool checkForRead = false;
    bool checkForWrite = false;
    
    int fd, tries_left = 4 /* kNoResRetry */;

    if (!isCreate)
    {
        switch (flags & O_ACCMODE) 
        {
            case O_RDONLY:
                checkForRead = true;
                break;
            case O_WRONLY:
                checkForWrite = true;
                break;
            case O_RDWR:
                checkForRead = true;
                checkForWrite = true;
                break;
        }

        if (checkForRead)
        {
            int result = sandbox_check(getpid(), "file-read-data", (sandbox_filter_type) (SANDBOX_FILTER_PATH | SANDBOX_CHECK_NO_REPORT), name);
            if (result != 0)
            {
                return -1;
            }
        }
        
        if (checkForWrite)
        {
            int result = sandbox_check(getpid(), "file-write-data", (sandbox_filter_type) (SANDBOX_FILTER_PATH | SANDBOX_CHECK_NO_REPORT), name);
            if (result != 0)
            {
                return -1;
            }
        }
    }

	do
	{
		fd = ::open(name, flags, mode);
	} while (fd < 0 && (errno == EINTR || (errno == ENFILE && --tries_left >= 0)));

	return fd;
}

int
AtomicFile::rclose(int fd)
{
	int result;
	do
	{
		result = ::close(fd);
	} while(result && errno == EINTR);

	return result;
}

//
// AtomicBufferedFile - This represents an instance of a file opened for reading.
// The file is read into memory and closed after this is done.
// The memory is released when this object is destroyed.
//
AtomicBufferedFile::AtomicBufferedFile(const std::string &inPath, bool isLocal) :
	mPath(inPath),
	mFileRef(-1),
	mBuffer(NULL),
	mLength(0),
	mIsMapped(isLocal)
{
}

AtomicBufferedFile::~AtomicBufferedFile()
{
	if (mFileRef >= 0)
	{
		AtomicFile::rclose(mFileRef);
		secdebug("atomicfile", "%p closed %s", this, mPath.c_str());
	}

	if (mBuffer)
	{
		secdebug("atomicfile", "%p free %s buffer %p", this, mPath.c_str(), mBuffer);
		unloadBuffer();
	}
}

//
// Open the file and return the length in bytes.
//
off_t
AtomicBufferedFile::open()
{
	const char *path = mPath.c_str();
	if (mFileRef >= 0)
	{
		secdebug("atomicfile", "open %s: already open, closing and reopening", path);
		close();
	}

	mFileRef = AtomicFile::ropen(path, O_RDONLY, 0);
    if (mFileRef == -1)
    {
        int error = errno;
		secdebug("atomicfile", "open %s: %s", path, strerror(error));

        // Do the obvious error code translations here.
		// @@@ Consider moving these up a level.
        if (error == ENOENT)
			CssmError::throwMe(CSSMERR_DL_DATASTORE_DOESNOT_EXIST);
		else if (error == EACCES)
			CssmError::throwMe(CSSM_ERRCODE_OS_ACCESS_DENIED);
		else
			UnixError::throwMe(error);
    }

	struct stat st;
	int result = fstat(mFileRef, &st);
	if (result == 0)
	{
		mLength = st.st_size;
	}
	else
	{
		int error = errno;
		secdebug("atomicfile", "lseek(%s, END): %s", path, strerror(error));
		AtomicFile::rclose(mFileRef);
		UnixError::throwMe(error);
	}

	secdebug("atomicfile", "%p opened %s: %qd bytes", this, path, mLength);

	return mLength;
}

//
// Unload the contents of the file.
//
void
AtomicBufferedFile::unloadBuffer()
{
	if (!mIsMapped)
	{
		delete [] mBuffer;
	}
	else
	{
		munmap(mBuffer, (size_t)mLength);
	}
}

//
// Load the contents of the file into memory.
// If we are on a local file system, we mmap the file.  Otherwise, we
// read it all into memory
void
AtomicBufferedFile::loadBuffer()
{
	if (!mIsMapped)
	{
		// make a buffer big enough to hold the entire file
		mBuffer = new uint8[mLength];
		lseek(mFileRef, 0, SEEK_SET);
		ssize_t pos = 0;
		
		ssize_t bytesToRead = (ssize_t)mLength;
		while (bytesToRead > 0)
		{
			ssize_t bytesRead = ::read(mFileRef, mBuffer + pos, bytesToRead);
			if (bytesRead == -1)
			{
				if (errno != EINTR)
				{
					int error = errno;
					secdebug("atomicfile", "lseek(%s, END): %s", mPath.c_str(), strerror(error));
					AtomicFile::rclose(mFileRef);
					UnixError::throwMe(error);
				}
			}
			else
			{
				bytesToRead -= bytesRead;
				pos += bytesRead;
			}
		}
	}
	else
	{
		// mmap the buffer into place
		mBuffer = (uint8*) mmap(NULL, (size_t)mLength, PROT_READ, MAP_PRIVATE, mFileRef, 0);
		if (mBuffer == (uint8*) -1)
		{
			int error = errno;
			secdebug("atomicfile", "lseek(%s, END): %s", mPath.c_str(), strerror(error));
			AtomicFile::rclose(mFileRef);
			UnixError::throwMe(error);
		}
	}
}



//
// Read the file starting at inOffset for inLength bytes into the buffer and return
// a pointer to it.  On return outLength contain the actual number of bytes read, it
// will only ever be less than inLength if EOF was reached, and it will never be more
// than inLength.
//
const uint8 *
AtomicBufferedFile::read(off_t inOffset, off_t inLength, off_t &outLength)
{
	if (mFileRef < 0)
	{
		secdebug("atomicfile", "read %s: file yet not opened, opening", mPath.c_str());
		open();
	}

	off_t bytesLeft = inLength;
	if (mBuffer)
	{
		secdebug("atomicfile", "%p free %s buffer %p", this, mPath.c_str(), mBuffer);
		unloadBuffer();
	}

	loadBuffer();
	
	secdebug("atomicfile", "%p allocated %s buffer %p size %qd", this, mPath.c_str(), mBuffer, bytesLeft);
	
	off_t maxEnd = inOffset + inLength;
	if (maxEnd > mLength)
	{
		maxEnd = mLength;
	}
	
	outLength = maxEnd - inOffset;
	
	return mBuffer + inOffset;
}

void
AtomicBufferedFile::close()
{
	if (mFileRef < 0)
	{
		secdebug("atomicfile", "close %s: already closed", mPath.c_str());
	}
	else
	{
		int result = AtomicFile::rclose(mFileRef);
		mFileRef = -1;
		if (result == -1)
		{
			int error = errno;
			secdebug("atomicfile", "close %s: %s", mPath.c_str(), strerror(errno));
			UnixError::throwMe(error);
		}

		secdebug("atomicfile", "%p closed %s", this, mPath.c_str());
	}
}


//
// AtomicTempFile - A temporary file to write changes to.
//
AtomicTempFile::AtomicTempFile(AtomicFile &inFile, const RefPointer<AtomicLockedFile> &inLockedFile, mode_t mode) :
	mFile(inFile),
	mLockedFile(inLockedFile),
	mCreating(true)
{
	create(mode);
}

AtomicTempFile::AtomicTempFile(AtomicFile &inFile, const RefPointer<AtomicLockedFile> &inLockedFile) :
	mFile(inFile),
	mLockedFile(inLockedFile),
	mCreating(false)
{
	create(mFile.mode());
}

AtomicTempFile::~AtomicTempFile()
{
	// rollback if we didn't commit yet.
	if (mFileRef >= 0)
		rollback();
}

//
// Open the file and return the length in bytes.
//
void
AtomicTempFile::create(mode_t mode)
{
	// we now generate our temporary file name through sandbox API's.
    
    // put the dir into a canonical form
    string dir = mFile.dir();
    int i = (int)dir.length() - 1;
    
    // walk backwards until we get to a non / character
    while (i >= 0 && dir[i] == '/')
    {
        i -= 1;
    }
    
    // point one beyond the string
    i += 1;
    
    const char* temp = _amkrtemp((dir.substr(0, i) + "/" + mFile.file()).c_str());
    if (temp == NULL)
    {
        UnixError::throwMe(errno);
    }
    
	mPath = temp;
    free((void*) temp);
    
	const char *path = mPath.c_str();

	mFileRef = AtomicFile::ropen(path, O_WRONLY|O_CREAT|O_TRUNC, mode);
    if (mFileRef == -1)
    {
        int error = errno;
		secdebug("atomicfile", "open %s: %s", path, strerror(error));

        // Do the obvious error code translations here.
		// @@@ Consider moving these up a level.
        if (error == EACCES)
			CssmError::throwMe(CSSM_ERRCODE_OS_ACCESS_DENIED);
		else
			UnixError::throwMe(error);
    }

	// If we aren't creating the inital file, make sure we preserve
	// the mode of the old file regardless of the current umask.
	// If we are creating the inital file we respect the users
	// current umask.
	if (!mCreating)
	{
		if (::fchmod(mFileRef, mode))
		{
			int error = errno;
			secdebug("atomicfile", "fchmod %s: %s", path, strerror(error));
			UnixError::throwMe(error);
		}
	}

	secdebug("atomicfile", "%p created %s", this, path);
}

void
AtomicTempFile::write(AtomicFile::OffsetType inOffsetType, off_t inOffset, const uint32 inData)
{
    uint32 aData = htonl(inData);
    write(inOffsetType, inOffset, reinterpret_cast<uint8 *>(&aData), sizeof(aData));
}

void
AtomicTempFile::write(AtomicFile::OffsetType inOffsetType, off_t inOffset,
				  const uint32 *inData, uint32 inCount)
{
#ifdef HOST_LONG_IS_NETWORK_LONG
    // Optimize this for the case where hl == nl
    const uint32 *aBuffer = inData;
#else
    auto_array<uint32> aBuffer(inCount);
    for (uint32 i = 0; i < inCount; i++)
        aBuffer.get()[i] = htonl(inData[i]);
#endif

    write(inOffsetType, inOffset, reinterpret_cast<const uint8 *>(aBuffer.get()),
    	  inCount * sizeof(*inData));
}

void
AtomicTempFile::write(AtomicFile::OffsetType inOffsetType, off_t inOffset, const uint8 *inData, size_t inLength)
{
	off_t pos;
	if (inOffsetType == AtomicFile::FromEnd)
	{
		pos = ::lseek(mFileRef, 0, SEEK_END);
		if (pos == -1)
		{
			int error = errno;
			secdebug("atomicfile", "lseek(%s, %qd): %s", mPath.c_str(), inOffset, strerror(error));
			UnixError::throwMe(error);
		}
	}
	else if (inOffsetType == AtomicFile::FromStart)
		pos = inOffset;
	else
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

	off_t bytesLeft = inLength;
	const uint8 *ptr = inData;
	while (bytesLeft)
	{
		size_t toWrite = bytesLeft > kAtomicFileMaxBlockSize ? kAtomicFileMaxBlockSize : size_t(bytesLeft);
		ssize_t bytesWritten = ::pwrite(mFileRef, ptr, toWrite, pos);
		if (bytesWritten == -1)
		{
			int error = errno;
			if (error == EINTR)
			{
				// We got interrupted by a signal, so try again.
				secdebug("atomicfile", "write %s: interrupted, retrying", mPath.c_str());
				continue;
			}

			secdebug("atomicfile", "write %s: %s", mPath.c_str(), strerror(error));
			UnixError::throwMe(error);
		}

		// Write returning 0 is bad mmkay.
		if (bytesWritten == 0)
		{
			secdebug("atomicfile", "write %s: 0 bytes written", mPath.c_str());
			CssmError::throwMe(CSSMERR_DL_INTERNAL_ERROR);
		}

		secdebug("atomicfile", "%p wrote %s %ld bytes from %p", this, mPath.c_str(), bytesWritten, ptr);

		bytesLeft -= bytesWritten;
		ptr += bytesWritten;
		pos += bytesWritten;
	}
}

void
AtomicTempFile::fsync()
{
	if (mFileRef < 0)
	{
		secdebug("atomicfile", "fsync %s: already closed", mPath.c_str());
	}
	else
	{
		int result;
		do
		{
			result = ::fsync(mFileRef);
		} while (result && errno == EINTR);

		if (result == -1)
		{
			int error = errno;
			secdebug("atomicfile", "fsync %s: %s", mPath.c_str(), strerror(errno));
			UnixError::throwMe(error);
		}

		secdebug("atomicfile", "%p fsynced %s", this, mPath.c_str());
	}
}

void
AtomicTempFile::close()
{
	if (mFileRef < 0)
	{
		secdebug("atomicfile", "close %s: already closed", mPath.c_str());
	}
	else
	{
		int result = AtomicFile::rclose(mFileRef);
		mFileRef = -1;
		if (result == -1)
		{
			int error = errno;
			secdebug("atomicfile", "close %s: %s", mPath.c_str(), strerror(errno));
			UnixError::throwMe(error);
		}

		secdebug("atomicfile", "%p closed %s", this, mPath.c_str());
	}
}

// Commit the current create or write and close the write file.  Note that a throw during the commit does an automatic rollback.
void
AtomicTempFile::commit()
{
	try
	{
		fsync();
		close();
		const char *oldPath = mPath.c_str();
		const char *newPath = mFile.path().c_str();

		// <rdar://problem/6991037>
		// Copy the security parameters of one file to another
		// Adding this to guard against setuid utilities that are re-writing a user's keychain.  We don't want to leave them root-owned.
		// In order to not break backward compatability we'll make a best effort, but continue if these efforts fail.
		//
		// To clear something up - newPath is the name the keychain will become - which is the name of the file being replaced
		//                         oldPath is the "temp filename".

		copyfile_state_t s;
		s = copyfile_state_alloc();

		if(copyfile(newPath, oldPath, s, COPYFILE_SECURITY | COPYFILE_NOFOLLOW) == -1) // Not fatal
			secdebug("atomicfile", "copyfile (%s, %s): %s", oldPath, newPath, strerror(errno));

		copyfile_state_free(s);
		// END <rdar://problem/6991037>

		::utimes(oldPath, NULL);

		if (::rename(oldPath, newPath) == -1)
		{
			int error = errno;
			secdebug("atomicfile", "rename (%s, %s): %s", oldPath, newPath, strerror(errno));
			UnixError::throwMe(error);
		}

		// Unlock the lockfile
		mLockedFile = NULL;

		secdebug("atomicfile", "%p commited %s", this, oldPath);
	}
	catch (...)
	{
		rollback();
		throw;
	}
}

// Rollback the current create or write (happens automatically if commit() isn't called before the destructor is.
void
AtomicTempFile::rollback() throw()
{
	if (mFileRef >= 0)
	{
		AtomicFile::rclose(mFileRef);
		mFileRef = -1;
	}

	// @@@ Log errors if this fails.
	const char *path = mPath.c_str();
	if (::unlink(path) == -1)
	{
		secdebug("atomicfile", "unlink %s: %s", path, strerror(errno));
		// rollback can't throw
	}

	// @@@ Think about this.  Depending on how we do locking we might not need this.
	if (mCreating)
	{
		const char *path = mFile.path().c_str();
		if (::unlink(path) == -1)
		{
			secdebug("atomicfile", "unlink %s: %s", path, strerror(errno));
			// rollback can't throw
		}
	}
}


//
// An advisory write lock for inFile.
//
FileLocker::~FileLocker()
{
}



LocalFileLocker::LocalFileLocker(AtomicFile &inFile) :
	mPath(inFile.lockFileName())
{
}


LocalFileLocker::~LocalFileLocker()
{
}



#ifndef NDEBUG
static double GetTime()
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return ((double) t.tv_sec) + ((double) t.tv_usec) / 1000000.0;
}
#endif



void
LocalFileLocker::lock(mode_t mode)
{
	struct stat st;

	do
	{
		// if the lock file doesn't exist, create it
		mLockFile = open(mPath.c_str(), O_RDONLY | O_CREAT, mode);
		
		// if we can't open or create the file, something is wrong
		if (mLockFile == -1)
		{
			UnixError::throwMe(errno);
		}
		
		// try to get exclusive access to the file
		IFDEBUG(double startTime = GetTime());
		int result = flock(mLockFile, LOCK_EX);
		IFDEBUG(double endTime = GetTime());
		
		IFDEBUG(secdebug("atomictime", "Waited %.4f milliseconds for file lock", (endTime - startTime) * 1000.0));
		
		// errors at this point are bad
		if (result == -1)
		{
			UnixError::throwMe(errno);
		}
		
		// check and see if the file we have access to still exists.  If not, another file shared our file lock
		// due to a hash collision and has thrown our lock away -- that, or a user blew the lock file away himself.
		
		result = fstat(mLockFile, &st);
		
		// errors at this point are bad
		if (result == -1)
		{
			UnixError::throwMe(errno);
		}
		
		if (st.st_nlink == 0) // we've been unlinked!
		{
			close(mLockFile);
		}
	} while (st.st_nlink == 0);
}


void
LocalFileLocker::unlock()
{
	flock(mLockFile, LOCK_UN);
	close(mLockFile);
}


	
NetworkFileLocker::NetworkFileLocker(AtomicFile &inFile) :
	mDir(inFile.dir()),
	mPath(inFile.dir() + "lck~" + inFile.file())
{
}

NetworkFileLocker::~NetworkFileLocker()
{
}

std::string
NetworkFileLocker::unique(mode_t mode)
{
	static const int randomPart = 16;
	DevRandomGenerator randomGen;
	std::string::size_type dirSize = mDir.size();
	std::string fullname(dirSize + randomPart + 2, '\0');
	fullname.replace(0, dirSize, mDir);
	fullname[dirSize] = '~'; /* UNIQ_PREFIX */
	char buf[randomPart];
	struct stat filebuf;
	int result, fd = -1;

	for (int retries = 0; retries < 10; ++retries)
	{
		/* Make a random filename. */
		randomGen.random(buf, randomPart);
		for (int ix = 0; ix < randomPart; ++ix)
		{
			char ch = buf[ix] & 0x3f;
			fullname[ix + dirSize + 1] = ch +
				( ch < 26            ? 'A'
				: ch < 26 + 26       ? 'a' - 26
				: ch < 26 + 26 + 10  ? '0' - 26 - 26
				: ch == 26 + 26 + 10 ? '-' - 26 - 26 - 10
				:                      '_' - 26 - 26 - 11);
		}

		result = lstat(fullname.c_str(), &filebuf);
		if (result && errno == ENAMETOOLONG)
		{
			do
				fullname.erase(fullname.end() - 1);
			while((result = lstat(fullname.c_str(), &filebuf)) && errno == ENAMETOOLONG && fullname.size() > dirSize + 8);
		}       /* either it stopped being a problem or we ran out of filename */

		if (result && errno == ENOENT)
		{
			fd = AtomicFile::ropen(fullname.c_str(), O_WRONLY|O_CREAT|O_EXCL, mode);
			if (fd >= 0 || errno != EEXIST)
				break;
		}
	}

	if (fd < 0)
	{
		int error = errno;
		::syslog(LOG_ERR, "Couldn't create temp file %s: %s", fullname.c_str(), strerror(error));
		secdebug("atomicfile", "Couldn't create temp file %s: %s", fullname.c_str(), strerror(error));
		UnixError::throwMe(error);
	}

	/* @@@ Check for EINTR. */
	write(fd, "0", 1); /* pid 0, `works' across networks */

	AtomicFile::rclose(fd);

	return fullname;
}

/* Return 0 on success and 1 on failure if st is set to the result of stat(old) and -1 on failure if the stat(old) failed. */
int
NetworkFileLocker::rlink(const char *const old, const char *const newn, struct stat &sto)
{
	int result = ::link(old,newn);
	if (result)
	{
		int serrno = errno;
		if (::lstat(old, &sto) == 0)
		{
			struct stat stn;
			if (::lstat(newn, &stn) == 0
				&& sto.st_dev == stn.st_dev
				&& sto.st_ino == stn.st_ino
				&& sto.st_uid == stn.st_uid
				&& sto.st_gid == stn.st_gid
				&& !S_ISLNK(sto.st_mode))
			{
				/* Link failed but files are the same so the link really went ok. */
				return 0;
			}
			else
				result = 1;
		}
		errno = serrno; /* Restore errno from link() */
	}

	return result;
}

/* NFS-resistant rename()
 * rename with fallback for systems that don't support it
 * Note that this does not preserve the contents of the file. */
int
NetworkFileLocker::myrename(const char *const old, const char *const newn)
{
	struct stat stbuf;
	int fd = -1;
	int ret;

	/* Try a real hardlink */
	ret = rlink(old, newn, stbuf);
	if (ret > 0)
	{
		if (stbuf.st_nlink < 2 && (errno == EXDEV || errno == ENOTSUP))
		{
			/* Hard link failed so just create a new file with O_EXCL instead.  */
			fd = AtomicFile::ropen(newn, O_WRONLY|O_CREAT|O_EXCL, stbuf.st_mode);
			if (fd >= 0)
				ret = 0;
		}
	}

	/* We want the errno from the link or the ropen, not that of the unlink. */
	int serrno = errno;

	/* Unlink the temp file. */
	::unlink(old);
	if (fd > 0)
		AtomicFile::rclose(fd);

	errno = serrno;
	return ret;
}

int
NetworkFileLocker::xcreat(const char *const name, mode_t mode, time_t &tim)
{
	std::string uniqueName = unique(mode);
	const char *uniquePath = uniqueName.c_str();
	struct stat stbuf;       /* return the filesystem time to the caller */
	stat(uniquePath, &stbuf);
	tim = stbuf.st_mtime;
	return myrename(uniquePath, name);
}

void
NetworkFileLocker::lock(mode_t mode)
{
	const char *path = mPath.c_str();
	bool triedforce = false;
	struct stat stbuf;
	time_t t, locktimeout = 1024; /* DEFlocktimeout, 17 minutes. */
	bool doSyslog = false;
	bool failed = false;
	int retries = 0;

	while (!failed)
	{
		/* Don't syslog first time through. */
		if (doSyslog)
			::syslog(LOG_NOTICE, "Locking %s", path);
		else
			doSyslog = true;

		secdebug("atomicfile", "Locking %s", path);          /* in order to cater for clock skew: get */
		if (!xcreat(path, mode, t))    /* time t from the filesystem */
		{
			/* lock acquired, hurray! */
			break;
		}
		switch(errno)
		{
		case EEXIST:               /* check if it's time for a lock override */
			if (!lstat(path, &stbuf) && stbuf.st_size <= 16 /* MAX_locksize */ && locktimeout
				&& !lstat(path, &stbuf) && locktimeout < t - stbuf.st_mtime)
				/* stat() till unlink() should be atomic, but can't guarantee that. */
			{
				if (triedforce)
				{
					/* Already tried, force lock override, not trying again */
					failed = true;
					break;
				}
				else if (S_ISDIR(stbuf.st_mode) || ::unlink(path))
				{
					triedforce=true;
					::syslog(LOG_ERR, "Forced unlock denied on %s", path);
					secdebug("atomicfile", "Forced unlock denied on %s", path);
				}
				else
				{
					::syslog(LOG_ERR, "Forcing lock on %s", path);
					secdebug("atomicfile", "Forcing lock on %s", path);
					sleep(16 /* DEFsuspend */);
					break;
				}
			}
			else
				triedforce = false;              /* legitimate iteration, clear flag */

			/* Reset retry counter. */
			retries = 0;
			usleep(250000);
			break;

		case ENOSPC:               /* no space left, treat it as a transient */
#ifdef EDQUOT                                                 /* NFS failure */
		case EDQUOT:                  /* maybe it was a short term shortage? */
#endif
		case ENOENT:
		case ENOTDIR:
		case EIO:
		/*case EACCES:*/
			if(++retries < (256 + 1))  /* nfsTRY number of times+1 to ignore spurious NFS errors */
				usleep(250000);
			else
				failed = true;
			break;

#ifdef ENAMETOOLONG
		case ENAMETOOLONG:     /* Filename is too long, shorten and retry */
			if (mPath.size() > mDir.size() + 8)
			{
				secdebug("atomicfile", "Truncating %s and retrying lock", path);
				mPath.erase(mPath.end() - 1);
				path = mPath.c_str();
				/* Reset retry counter. */
				retries = 0;
				break;
			}
		/* DROPTHROUGH */
#endif
		default:
			failed = true;
			break;
		}
	}

	if (failed)
	{
		int error = errno;
		::syslog(LOG_ERR, "Lock failure on %s: %s", path, strerror(error));
		secdebug("atomicfile", "Lock failure on %s: %s", path, strerror(error));
		UnixError::throwMe(error);
	}
}

void
NetworkFileLocker::unlock()
{
	const char *path = mPath.c_str();
	if (::unlink(path) == -1)
	{
		secdebug("atomicfile", "unlink %s: %s", path, strerror(errno));
		// unlock can't throw
	}
}



AtomicLockedFile::AtomicLockedFile(AtomicFile &inFile)
{
	if (inFile.isOnLocalFileSystem())
	{
		mFileLocker = new LocalFileLocker(inFile);
	}
	else
	{
		mFileLocker = new NetworkFileLocker(inFile);
	}
	
	lock();
}



AtomicLockedFile::~AtomicLockedFile()
{
	unlock();
	delete mFileLocker;
}



void
AtomicLockedFile::lock(mode_t mode)
{
	mFileLocker->lock(mode);
}



void AtomicLockedFile::unlock() throw()
{
	mFileLocker->unlock();
}



#undef kAtomicFileMaxBlockSize
