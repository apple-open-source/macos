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
//  AtomicFile.h - Description t.b.d.
//
#ifndef _H_ATOMICFILE
#define _H_ATOMICFILE

#include <Security/threading.h>

#include <map>
#include <string>

#if _USE_IO == _USE_IO_POSIX
#include <sys/types.h>
#include <machine/endian.h>
#elif _USE_IO == _USE_IO_MACOS
#define htonl(X) (X)
#define ntohl(X) (X)
#endif

#ifdef _CPP_ATOMICFILE
#pragma export on
#endif

namespace Security
{

class DbName;

class AtomicFile
{
public:
    typedef int FileRef;
    typedef int VersionId;

    AtomicFile(const DbName &inDbName);
	~AtomicFile();

    // Close the currently open AtomicFile.  (If there are transactions outstanding this call
    // has no effect until after they have completed.
    void close();

    // Start a read operation. Returns a mmaped region with the file in it.  Return the size of the
    // file in length.  Each call to enterRead() *must* be paired with a call to exitRead.
    VersionId enterRead(const uint8 *&outFileAddress, size_t &outLength);

    // End a read operation.
    void exitRead(VersionId inVersionId);

	// Return true if inVersionId is not the most recent version of this file.
	bool isDirty(VersionId inVersionId);

    // Aquire the write lock and remove the file.
    void performDelete();

    // Create and lock the database file for writing, and set outWriteRef to a newly created
    // file open for writing.
    // Return the new VersionId this file will have after a succesful commit.
    VersionId enterCreate(FileRef &outWriteRef);

    // Lock the database file for writing, map the database file for reading and
    // set outWriteRef to a newly created file open for writing.
    // Return the VersionId or the file being modified.
    VersionId enterWrite(const uint8 *&outFileAddress, size_t &outLength, FileRef &outWriteRef);

    // Commit the current create or write and close the write file.  Return the VersionId of the new file.
    VersionId commit();

    // Rollback the current create or write.
    void rollback();

    enum OffsetType {
        None,
        FromStart,
        FromCurrent
    };

    void write(OffsetType inOffsetType, uint32 inOffset, const uint32 *inData, uint32 inCount);
    void write(OffsetType inOffsetType, uint32 inOffset, const uint8 *inData, uint32 inLength);
    void write(OffsetType inOffsetType, uint32 inOffset, const uint32 inData);
    const string filename() const { return mReadFilename; }
private:
    void endWrite();
    void rename(const string &inSrcFilename, const string &inDestFilename);
    void unlink(const string &inFilename);

    class OpenFile
    {
    public:
        OpenFile(const std::string &inFilename, bool write, bool lock, VersionId inVersionId, mode_t mode);
        ~OpenFile();

        void close();
        VersionId versionId() const { return mVersionId; }
        FileRef fileRef() const { return mFileRef; }
        const uint8 *address() const { return mAddress; }
        size_t length() const { return mLength; }

        // Check if the file has its dirty bit set.
        bool isDirty();
        // Set the files dirty bit (requires the file to be writeable and locked).
        void setDirty();

        void lock();
        void unlock();

		// Return the mode bits of the file
		mode_t mode();

        int mUseCount;
        FileRef mFileRef;
    private:
        VersionId readVersionId();
        void writeVersionId(VersionId inVersionId);
		static void mkpath(const std::string &inFilename);

        VersionId mVersionId;
        const uint8 *mAddress;
        size_t mLength;
		bool mFcntlLock;
        enum
        {
            Closed,
            Read,
            Write,
            ReadWrite,
            Create
        } mState;
    };

    Mutex mReadLock;
    OpenFile *mReadFile;
    string mReadFilename;

    Mutex mWriteLock;
    OpenFile *mWriteFile;
    string mWriteFilename;

    typedef std::map<VersionId, OpenFile *> OpenFileMap;
    OpenFileMap mOpenFileMap;

    bool mCreating;
};


class AtomicFileRef
{
public:
    virtual ~AtomicFileRef();

    uint32 at(uint32 inOffset)
    {
        return ntohl(*reinterpret_cast<const uint32 *>(mAddress + inOffset));
    }

    uint32 operator[](uint32 inOffset)
    {
        if (inOffset + sizeof(uint32) > mLength)
            CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
        return at(inOffset);
    }

    const uint8 *range(uint32 inOffset, uint32 inLength)
    {
        if (inOffset + inLength > mLength)
            CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
        return mAddress + inOffset;
    }

    const AtomicFile::VersionId mVersionId;
protected:
    struct InitArg;
    AtomicFileRef(AtomicFile &inAtomicFile, const InitArg &inInitArg);

    AtomicFile &mAtomicFile;
    const uint8 *mAddress;
    const size_t mLength;
};

// Use this class to open an AtomicFile for reading.
class AtomicFileReadRef : public AtomicFileRef
{
public:
    AtomicFileReadRef(AtomicFile &inAtomicFile);
    virtual ~AtomicFileReadRef();
private:
    static InitArg enterRead(AtomicFile &inAtomicFile);
};

// Use this class to open an AtomicFile for writing.
class AtomicFileWriteRef : public AtomicFileRef
{
public:
    AtomicFileWriteRef(AtomicFile &inAtomicFile);
    virtual ~AtomicFileWriteRef();
    AtomicFile::VersionId commit() { mOpen = false; return mAtomicFile.commit(); }

private:
    static InitArg enterWrite(AtomicFile &inAtomicFile, AtomicFile::FileRef &outWriteFileRef);
    AtomicFile::FileRef mFileRef;
    bool mOpen;
};

} // end namespace Security

#ifdef _CPP_ATOMICFILE
#pragma export off
#endif

#endif //_H_ATOMICFILE
