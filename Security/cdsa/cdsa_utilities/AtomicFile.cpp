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
//  AtomicFile.cpp - Description t.b.d.
//
#ifdef __MWERKS__
#define _CPP_ATOMICFILE
#endif

#include <Security/AtomicFile.h>
#include <Security/DbName.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>
#include <stack>

#if _USE_IO == _USE_IO_POSIX
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <sys/stat.h>
//#include <err.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#elif _USE_IO == _USE_IO_MACOS
typedef SInt32 ssize_t;
#endif

using namespace std;

AtomicFile::AtomicFile(const DbName &inDbName) :
    mReadFile(nil),
    mReadFilename(inDbName.dbName()),
    mWriteFile(nil),
    mWriteFilename(mReadFilename + ",") // XXX Do some more work here like resolving symlinks/aliases etc.
{
    // We only support databases with string names of non-zero length.
    if (inDbName.dbLocation() != nil || inDbName.dbName().length() == 0)
        CssmError::throwMe(CSSMERR_DL_INVALID_DB_LOCATION);
}

AtomicFile::~AtomicFile()
{
    // Assume there are no more running theads in this object.

    // Try hard to clean up as much as possible.
    try
    {
        // Rollback any pending write.
        if (mWriteFile)
            rollback();
    }
    catch(...) {}

    // Close and delete all files in mOpenFileMap
    for (OpenFileMap::iterator it = mOpenFileMap.begin(); it != mOpenFileMap.end(); it++)
    {
        try
        {
            it->second->close();
        }
        catch(...) {}
        try
        {
            delete it->second;
        }
        catch(...) {}
    }
}

void
AtomicFile::close()
{
    StLock<Mutex> _(mReadLock);

    // If we have no read file we have nothing to close.
    if (mReadFile == nil)
        return;

    // Remember mReadFile and set it to nil, so that it will be closed after any pending write completes
    OpenFile *aOpenFile = mReadFile;
    mReadFile = nil;

    // If aOpenFile has a zero use count no other thread is currently using it,
    // so we can safely remove it from the map.
    if (aOpenFile->mUseCount == 0)
    {
        // Do not close any files (nor remove them from the map) while some thread is writing
        // since doing so might release the lock we are holding.
        if (mWriteLock.tryLock())
        {
            // Release the write lock immediately since tryLock just aquired it and we don't want to write.
            mWriteLock.unlock();

            // Remove aOpenFile from the map of open files.
            mOpenFileMap.erase(aOpenFile->versionId());
            try
            {
                aOpenFile->close();
            }
            catch(...)
            {
                delete aOpenFile;
                throw;
            }
            delete aOpenFile;
        }
    }
}

AtomicFile::VersionId
AtomicFile::enterRead(const uint8 *&outFileAddress, size_t &outLength)
{
    StLock<Mutex> _(mReadLock);

    // If we already have a read file check if it is still current.
    if (mReadFile != nil)
    {
        if (mReadFile->isDirty())
        {
            // Remember mReadFile and set it to nil in case an exception is thrown
            OpenFile *aOpenFile = mReadFile;
            mReadFile = nil;

            // If aOpenFile has a zero use count no other thread is currently using it,
            // so we can safely remove it from the map.
            if (aOpenFile->mUseCount == 0)
            {
                // Do not close any files (nor remove them from the map) while some thread is writing
                // since doing so might release the lock we are holding.
                if (mWriteLock.tryLock())
                {
                    // Release the write lock immediately since tryLock just aquired it and we don't want to write.
                    mWriteLock.unlock();

                    // Remove aOpenFile from the map of open files.
                    mOpenFileMap.erase(aOpenFile->versionId());
                    try
                    {
                        aOpenFile->close();
                    }
                    catch(...)
                    {
                        delete aOpenFile;
                        throw;
                    }
                    delete aOpenFile;
                }
            }
        }
    }

    // If we never had or no longer have an open read file.  Open it now.
    if (mReadFile == nil)
    {
        mReadFile = new OpenFile(mReadFilename, false, false, 0);
        mOpenFileMap.insert(OpenFileMap::value_type(mReadFile->versionId(), mReadFile));
    }
    // Note that mReadFile->isDirty() might actually return true here, but all that mean is
    // that we are looking at data that was commited after we opened the file which might
    // happen in a few miliseconds anyway.

    // Bump up the use count of our OpenFile.
    mReadFile->mUseCount++;

    // Return the length of the file and the mapped address.
    outLength = mReadFile->length();
    outFileAddress = mReadFile->address();
    return mReadFile->versionId();
}

void
AtomicFile::exitRead(VersionId inVersionId)
{
    StLock<Mutex> _(mReadLock);
    OpenFileMap::iterator it = mOpenFileMap.find(inVersionId);
    // If the inVersionId is not in the map anymore something really bad happned.
    if (it == mOpenFileMap.end())
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

    OpenFile *aOpenFile = it->second;
    aOpenFile->mUseCount--;

    // Don't close the current active file even if its mUseCount hits 0 since someone
    // else will probably request it soon.
    if (aOpenFile->mUseCount == 0 && aOpenFile != mReadFile)
    {
        // Do not close any files (nor remove them from the map) while some thread is writing
        // since doing so might release the lock we are holding.
        if (mWriteLock.tryLock())
        {
            // Release the write lock immidiatly since tryLock just aquired it and we don't want to write.
            mWriteLock.unlock();

            // Remove from the map, close and delete aOpenFile.
            mOpenFileMap.erase(it);
            try
            {
                aOpenFile->close();
            }
            catch(...)
            {
                delete aOpenFile;
                throw;
            }
            delete aOpenFile;
        }
    }
}

bool AtomicFile::isDirty(VersionId inVersionId)
{
    StLock<Mutex> _(mReadLock);
    OpenFileMap::iterator it = mOpenFileMap.find(inVersionId);
    // If the inVersionId is not in the map anymore something really bad happned.
    if (it == mOpenFileMap.end())
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

    return it->second->isDirty();
}

void
AtomicFile::performDelete()
{
    // Prevent any other threads in this process from writing.
    mWriteLock.lock();

    OpenFile *aReadFile = nil;
    try
    {
        // Keep reopening mReadFilename until the lock has been aquired on a non-dirty file.
        // XXX This is a potential infinite loop.
        for (;;)
        {
            aReadFile = new OpenFile(mReadFilename, true, true, 0);
            if (!aReadFile->isDirty())
                break;

            aReadFile->close();
            delete aReadFile;
            aReadFile = nil;
        }

        // Aquire the read lock so no other thread will open the file
        StLock<Mutex> _(mReadLock);

        // Delete the file.
        unlink(mReadFilename);

        // Clear our current mReadFile since it refers to the deleted file.
        mReadFile = nil;

        // Mark the old file as modified
        aReadFile->setDirty();

        // Close any open files.
        endWrite();
    }
    catch(...)
    {
        if (aReadFile)
        {
            try
            {
                VersionId aVersionId = aReadFile->versionId();
                aReadFile->close();
                mOpenFileMap.erase(aVersionId);
            } catch(...) {}
            delete aReadFile;
        }
        endWrite();
        throw;
    }
    endWrite();
}

AtomicFile::VersionId
AtomicFile::enterCreate(FileRef &outWriteRef)
{
    // Prevent any other threads in this process from writing.
    mWriteLock.lock();
    OpenFile *aReadFile = nil;
    try
    {
        // No threads can read during creation
        StLock<Mutex> _(mReadLock);

        // Create mReadFilename until the lock has been aquired on a non-dirty file.
        aReadFile = new OpenFile(mReadFilename, false, true, 1);

        // Open mWriteFile for writing.
        mWriteFile = new OpenFile(mWriteFilename, true, false, aReadFile->versionId() + 1);

        // Insert aReadFile into the map (do this after opening mWriteFile just in case that throws).
        mOpenFileMap.insert(OpenFileMap::value_type(-1, aReadFile));

        outWriteRef = mWriteFile->fileRef();
        mCreating = true; // So rollback() will delete mReadFileName.
        return aReadFile->versionId();
    }
    catch(...)
    {
        // Make sure we don't thow during cleanup since that would clobber the original
        // error and prevent us from releasing mWriteLock
        try
        {
            if (aReadFile)
            {
                try
                {
                    aReadFile->close();
                    // XXX We should only unlink if we know that no one else is currently creating the file.
                    //unlink(mReadFilename);
                    mOpenFileMap.erase(-1);
                } catch(...) {}
                delete aReadFile;
            }

            if (mWriteFile)
            {
                try
                {
                    mWriteFile->close();
                    unlink(mWriteFilename);
                } catch(...) {}
                delete mWriteFile;
                mWriteFile = nil;
            }
        }
        catch(...) {} // Do not throw since we already have an error.

        // Release the write lock and remove any unused files from the map
        endWrite();
        throw;
    }
}

AtomicFile::VersionId
AtomicFile::enterWrite(const uint8 *&outFileAddress, size_t &outLength, FileRef &outWriteRef)
{
    // Wait for all other threads in this process to finish writing.
    mWriteLock.lock();
    mCreating = false; // So rollback() will not delete mReadFileName.
    OpenFile *aReadFile = nil;
    try
    {
        // Keep reopening mReadFilename until the lock has been aquired on a non-dirty file.
        // XXX This is a potential infinite loop.
        for (;;)
        {
            aReadFile = new OpenFile(mReadFilename, true, true, 0);
            if (!aReadFile->isDirty())
                break;

            aReadFile->close();
            delete aReadFile;
            aReadFile = nil;
        }

        // We have the write lock on the file now we start modifying our shared data
        // stuctures so aquire the read lock.
        StLock<Mutex> _(mReadLock);

        // Open mWriteFile for writing.
        mWriteFile = new OpenFile(mWriteFilename, true, false, aReadFile->versionId() + 1);

        // Insert aReadFile into the map (do this after opening mWriteFile just in case that throws).
        mOpenFileMap.insert(OpenFileMap::value_type(-1, aReadFile));

        outWriteRef = mWriteFile->fileRef();
        outLength = aReadFile->length();
        outFileAddress = aReadFile->address();
        return aReadFile->versionId();
    }
    catch(...)
    {
        // Make sure we don't thow during cleanup since that would clobber the original
        // error and prevent us from releasing mWriteLock
        try
        {
            if (aReadFile)
            {
                try
                {
                    aReadFile->close();
                    mOpenFileMap.erase(-1);
                } catch(...) {}
                delete aReadFile;
            }

            if (mWriteFile)
            {
                try
                {
                    mWriteFile->close();
                    unlink(mWriteFilename);
                } catch(...) {}
                delete mWriteFile;
                mWriteFile = nil;
            }
        }
        catch(...) {} // Do not throw since we already have an error.

        // Release the write lock and remove any unused files from the map
        endWrite();
        throw;
    }
}

AtomicFile::VersionId
AtomicFile::commit()
{
    StLock<Mutex> _(mReadLock);
    if (mWriteFile == nil)
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

    try
    {
        VersionId aVersionId = mWriteFile->versionId();
        mWriteFile->close();
        delete mWriteFile;
        mWriteFile = nil;

        OpenFileMap::iterator it = mOpenFileMap.find(-1);
        if (it == mOpenFileMap.end())
            CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

        // First rename the file and them mark the old one as modified
        rename(mWriteFilename, mReadFilename);
        OpenFile *aOpenFile = it->second;

        // Clear our current mReadFile since it refers to the old file.
        mReadFile = nil;

        // Mark the old file as modified
        aOpenFile->setDirty();

        // Close all unused files (in particular aOpenFile) and remove them from mOpenFileMap
        endWrite();
        return aVersionId;
    }
    catch (...)
    {
        // Unlink the new file to rollback the transaction and close any open files.
        try
        {
            unlink(mWriteFilename);
        }catch(...) {}
        endWrite();
        throw;
    }
}

void
AtomicFile::rollback()
{
    StLock<Mutex> _(mReadLock);
    if (mWriteFile == nil)
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

    try
    {
        mWriteFile->close();
        delete mWriteFile;
        mWriteFile = nil;

        // First rename the file and them mark the old one as modified
        unlink(mWriteFilename);
        if (mCreating)
            unlink(mReadFilename);
        endWrite();
    }
    catch(...)
    {
        // Unlink the new file to rollback the transaction and close any open files.
        try
        {
            unlink(mWriteFilename);
        }catch(...) {}
        endWrite();
        throw;
    }
}

// This private function is called by a successfull commit(), rollback() or performDelete() as well
// as by a failed enterWrite() or enterCreate().
void
AtomicFile::endWrite()
{
    try
    {
        // We need to go in and close and delete all unused files from the queue
        stack<VersionId> aDeleteList;
        OpenFileMap::iterator it;
        for (it = mOpenFileMap.begin();
             it != mOpenFileMap.end();
             it++)
        {
            OpenFile *aOpenFile = it->second;
            // If aOpenFile is unused and it is not the mReadFile schedule it for close and removal.
            // Note that if this is being called after a commit mReadFile will have been set to nil.
            if (aOpenFile != mReadFile && aOpenFile->mUseCount == 0)
                aDeleteList.push(it->first);
        }

        // Remove everything that was scheduled for removal
        while (!aDeleteList.empty())
        {
            it = mOpenFileMap.find(aDeleteList.top());
            aDeleteList.pop();
            try
            {
                it->second->close();
            }
            catch(...) {}
            delete it->second;
            mOpenFileMap.erase(it);
        }

        if (mWriteFile)
        {
            mWriteFile->close();
        }
    }
    catch(...)
    {
        delete mWriteFile;
        mWriteFile = nil;
        mWriteLock.unlock();
        throw;
    }

    delete mWriteFile;
    mWriteFile = nil;
    mWriteLock.unlock();
}

void
AtomicFile::rename(const string &inSrcFilename, const string &inDestFilename)
{
    if (::rename(inSrcFilename.c_str(), inDestFilename.c_str()))
        UnixError::throwMe(errno);
}

void
AtomicFile::unlink(const string &inFilename)
{
    if (::unlink(inFilename.c_str()))
        UnixError::throwMe(errno);
}

void
AtomicFile::write(OffsetType inOffsetType, uint32 inOffset, const uint32 inData)
{
    uint32 aData = htonl(inData);
    write(inOffsetType, inOffset, reinterpret_cast<uint8 *>(&aData), sizeof(aData));
}

void
AtomicFile::write(OffsetType inOffsetType, uint32 inOffset,
				  const uint32 *inData, uint32 inCount)
{
#ifdef HOST_LONG_IS_NETWORK_LONG
    // XXX Optimize this for the case where hl == nl
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
AtomicFile::write(OffsetType inOffsetType, uint32 inOffset, const uint8 *inData, uint32 inLength)
{
    // Seriously paranoid check.
    if (mWriteFile == nil)
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

    if (inOffsetType != None)
    {
        if (::lseek(mWriteFile->mFileRef, inOffset, inOffsetType == FromStart ? SEEK_SET : SEEK_CUR) == -1)
            UnixError::throwMe(errno);
    }

    if (::write(mWriteFile->mFileRef, reinterpret_cast<const char *>(inData),
    			inLength) != static_cast<ssize_t>(inLength))
        UnixError::throwMe(errno);
}

// AtomicFile::OpenFile implementation

AtomicFile::OpenFile::OpenFile(const string &inFilename, bool write, bool lock, VersionId inVersionId) :
    mUseCount(0),
    mVersionId(inVersionId),
    mAddress(NULL),
    mLength(0)
{
    int flags, mode = 0;
    if (write && lock)
    {
        flags = O_RDWR;
        mState = ReadWrite;
    }
    else if (write && !lock)
    {
        flags = O_WRONLY|O_CREAT|O_TRUNC;
        mode = 0666;
        mState = Write;
    }
    else if (!write && lock)
    {
        flags = O_WRONLY|O_CREAT|O_TRUNC|O_EXCL;
        mode = 0666;
        mState = Create;
    }
    else
    {
        flags = O_RDONLY;
        mState = Read;
    }

    mFileRef = ::open(inFilename.c_str(), flags, mode);
    if (mFileRef == -1)
    {
        int error = errno;

#if _USE_IO == _USE_IO_POSIX
        // Do the obvious error code translations here.
        if (error == ENOENT)
		{
			// Throw CSSMERR_DL_DATASTORE_DOESNOT_EXIST even in Write state since it means someone threw away our parent directory.
			if (mState == ReadWrite || mState == Read || mState == Write)
				CssmError::throwMe(CSSMERR_DL_DATASTORE_DOESNOT_EXIST);
			if (mState == Create)
			{
				// Attempt to create the path to inFilename since one or more of the directories
				// in the path do not yet exist.
				mkpath(inFilename);

				// Now try the open again.
				mFileRef = ::open(inFilename.c_str(), flags, mode);
				error = mFileRef == -1 ? errno : 0;
				if (error == ENOENT)
					CssmError::throwMe(CSSM_ERRCODE_OS_ACCESS_DENIED);
			}
		}

		if (error == EACCES)
			CssmError::throwMe(CSSM_ERRCODE_OS_ACCESS_DENIED);

        if (error == EEXIST)
            CssmError::throwMe(CSSMERR_DL_DATASTORE_ALREADY_EXISTS);
#endif

		// Check if we are still in an error state.
		if (error)
			UnixError::throwMe(errno);
    }

    // If this is a new file write out the versionId
    if (mState == Create)
        writeVersionId(mVersionId);

    // If this is a temp output file we are done.
    if (mState == Write)
        return;

    try
    {
        mLength = ::lseek(mFileRef, 0, SEEK_END);
        if (mLength == static_cast<size_t>(-1))
            UnixError::throwMe(errno);
        if (mLength == 0)
        {
            // XXX What to set versionId to?
            mVersionId = 0;
            return; // No point in mapping a zero length file.
        }

#if _USE_IO == _USE_IO_POSIX
        // Lock the file if required.
        if (lock)
        {
            struct flock mLock;
            mLock.l_start = 0;
            mLock.l_len = 1;
            mLock.l_pid = getpid();
            mLock.l_type = F_WRLCK;
            mLock.l_whence = SEEK_SET;

            // Keep trying to obtain the lock if we get interupted.
            for (;;)
            {
                if (::fcntl(mFileRef, F_SETLKW, reinterpret_cast<int>(&mLock)) == -1)
                {
                    int error = errno;
                    if (error == EINTR)
                        continue;

					if (error != ENOTSUP)
						UnixError::throwMe(error);

					// XXX Filesystem does not support locking with fcntl use an alternative.
					mFcntlLock = false;
                }
				else
					mFcntlLock = true;

                break;
            }
        }

        if (mState != Create)
        {
            mAddress = reinterpret_cast<const uint8 *>
				(::mmap(0, mLength, PROT_READ, MAP_FILE|MAP_SHARED,
						mFileRef, 0));
            if (mAddress == reinterpret_cast<const uint8 *>(-1))
            {
                 mAddress = NULL;
                UnixError::throwMe(errno);
            }

            mVersionId = readVersionId();
        }
#else
        if (mState != Create)
        {
			mAddress = reinterpret_cast<const uint8 *>(-1);
        	auto_array<char> aBuffer(mLength);
 			if (::read(mFileRef, aBuffer.get(), mLength) != mLength)
 				UnixError::throwMe(errno);

            mAddress = reinterpret_cast<const uint8 *>(aBuffer.release());
            mVersionId = readVersionId();
        }
#endif
    }
    catch(...)
    {
        if (mState != Closed)
            ::close(mFileRef);
        throw;
    }
}

AtomicFile::OpenFile::~OpenFile()
{
    close();
}

void
AtomicFile::OpenFile::close()
{
    int error = 0;
    if (mAddress != NULL)
    {
#if _USE_IO == _USE_IO_POSIX
        if (::munmap(const_cast<uint8 *>(mAddress), mLength) == -1)
            error = errno;
#else
		delete[] mAddress;
#endif

        mAddress = NULL;
    }

    if (mState == Write)
        writeVersionId(mVersionId);

    if (mState != Closed)
    {
        mState = Closed;
        if (::close(mFileRef) == -1)
            error = errno;
    }

    if (error != 0)
        UnixError::throwMe(error);
}

bool
AtomicFile::OpenFile::isDirty()
{
    if (mAddress == NULL)
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

    return (mVersionId != readVersionId()) || mVersionId == 0;
}

// Set the files dirty bit (requires the file to be writeable and locked).
void
AtomicFile::OpenFile::setDirty()
{
    if (mState != ReadWrite && mState != Create)
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

    writeVersionId(0);
}

void
AtomicFile::OpenFile::unlock()
{
// XXX This should be called.
#if 0
	if (mFcntlLock)
	{
		struct flock mLock;
		mLock.l_start = 0;
		mLock.l_len = 1;
		mLock.l_pid = getpid();
		mLock.l_type = F_UNLCK;
		mLock.l_whence = SEEK_SET;
		if (::fcntl(mFileRef, F_SETLK, reinterpret_cast<int>(&mLock)) == -1)
			UnixError::throwMe(errno);
	}
#endif
}

AtomicFile::VersionId
AtomicFile::OpenFile::readVersionId()
{
    const uint8 *ptr;
    char buf[4];

    // Read the VersionId
    if (mAddress == NULL)
    {
        // Seek to the end of the file minus 4
		if (mLength < 4)
			CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

        if (::lseek(mFileRef, mLength - 4, SEEK_SET) == -1)
            UnixError::throwMe(errno);

        ptr = reinterpret_cast<uint8 *>(buf);
        if (::read(mFileRef, buf, 4) != 4)
            UnixError::throwMe(errno);
    }
    else
    {
        ptr = mAddress + mLength - 4;
        if (mLength < 4)
            CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
    }

    VersionId aVersionId = 0;
    for (int i = 0; i < 4; i++)
    {
        aVersionId = (aVersionId << 8) + ptr[i];
    }

    return aVersionId;
}

void
AtomicFile::OpenFile::writeVersionId(VersionId inVersionId)
{
	if (mState == ReadWrite)
	{
        // Seek to the end of the file minus 4
		if (mLength < 4)
			CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

        if (::lseek(mFileRef, mLength - 4, SEEK_SET) == -1)
            UnixError::throwMe(errno);
	}
	else /* if (mState == Create || mState == Write) */
	{
		// Seek to the end of the file.
		if (::lseek(mFileRef, 0, SEEK_END) == -1)
			UnixError::throwMe(errno);
	}

    uint8 buf[4];
    // Serialize the VersionId
    for (int i = 3; i >= 0; i--)
    {
        buf[i] = inVersionId & 0xff;
        inVersionId = inVersionId >> 8;
    }

    // Write the VersionId
    if (::write(mFileRef, reinterpret_cast<char *>(buf), 4) != 4)
        UnixError::throwMe(errno);
}

void
AtomicFile::OpenFile::mkpath(const std::string &inFilename)
{
	char *path = const_cast<char *>(inFilename.c_str()); // @@@ Const_cast is a lie!!!
	struct stat sb;
	char *slash;
    mode_t dir_mode = (0777 & ~umask(0)) | S_IWUSR | S_IXUSR;

	slash = path;

	for (;;)
	{
		slash += strspn(slash, "/");
		slash += strcspn(slash, "/");

		if (*slash == '\0')
			break;

		*slash = '\0';

		if (stat(path, &sb))
		{
			if (errno != ENOENT || mkdir(path, dir_mode))
				UnixError::throwMe(errno);
			/* The mkdir() and umask() calls both honor only the low
			   nine bits, so if you try to set a mode including the
			   sticky, setuid, setgid bits you lose them. So chmod().  */
			if (chmod(path, dir_mode) == -1)
				UnixError::throwMe(errno);
		}
		else if (!S_ISDIR(sb.st_mode))
			CssmError::throwMe(CSSM_ERRCODE_OS_ACCESS_DENIED);  // @@@ Should be is a directory

		*slash = '/';
	}
}



// Constructor uglyness to work around C++ language limitations.
struct AtomicFileRef::InitArg
{
    AtomicFile::VersionId versionId;
    const uint8 *address;
    size_t length;
};

AtomicFileRef::~AtomicFileRef()
{
}

AtomicFileRef::AtomicFileRef(AtomicFile &inAtomicFile, const InitArg &inInitArg) :
    mVersionId(inInitArg.versionId),
    mAtomicFile(inAtomicFile),
    mAddress(inInitArg.address),
    mLength(inInitArg.length)
{
}

AtomicFileReadRef::~AtomicFileReadRef()
{
	try {
		mAtomicFile.exitRead(mVersionId);
	}
	catch(...) {
	}
}

AtomicFileRef::InitArg
AtomicFileReadRef::enterRead(AtomicFile &inAtomicFile)
{
    InitArg anInitArg;
    anInitArg.versionId = inAtomicFile.enterRead(anInitArg.address, anInitArg.length);
    return anInitArg;
}

AtomicFileReadRef::AtomicFileReadRef(AtomicFile &inAtomicFile) :
    AtomicFileRef(inAtomicFile, enterRead(inAtomicFile))
{
}

AtomicFileWriteRef::~AtomicFileWriteRef()
{
	if (mOpen) {
		try {
			mAtomicFile.rollback();
		}
		catch (...) 
		{
		}
	}
}

AtomicFileRef::InitArg
AtomicFileWriteRef::enterWrite(AtomicFile &inAtomicFile, AtomicFile::FileRef &outWriteFileRef)
{
    InitArg anInitArg;
    anInitArg.versionId = inAtomicFile.enterWrite(anInitArg.address, anInitArg.length, outWriteFileRef);
    return anInitArg;
}

AtomicFileWriteRef::AtomicFileWriteRef(AtomicFile &inAtomicFile) :
    AtomicFileRef(inAtomicFile, enterWrite(inAtomicFile, mFileRef))
{
}
