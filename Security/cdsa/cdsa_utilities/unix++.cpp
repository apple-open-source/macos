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
// unix++ - C++ layer for basic UNIX facilities
//
#include "unix++.h"
#include <Security/debugging.h>


namespace Security {
namespace UnixPlusPlus {


//
// Generic UNIX file descriptors
//
void FileDesc::open(const char *path, int flags, mode_t mode)
{
    checkSetFd(::open(path, flags, mode));
    mAtEnd = false;
    secdebug("unixio", "open(%s,0x%x,0x%x) = %d", path, flags, mode, mFd);
}

void FileDesc::close()
{
    if (mFd >= 0) {
        checkError(::close(mFd));
        secdebug("unixio", "close(%d)", mFd);
        mFd = invalidFd;
    }
}


//
// Filedescoid operations
//
size_t FileDesc::read(void *addr, size_t length)
{
    switch (ssize_t rc = ::read(mFd, addr, length)) {
    case 0:		// end-of-source
        if (length == 0) { // check for errors, but don't set mAtEnd unless we have to
            secdebug("unixio", "%d zero read (ignored)", mFd);
            return 0;
        }
        mAtEnd = true;
        secdebug("unixio", "%d end of data", mFd);
        return 0;
    case -1:	// error
        if (errno == EAGAIN)
            return 0;	// no data, unknown end-of-source status
        UnixError::throwMe(); // throw error
    default:	// have data
        return rc;
    }
}

size_t FileDesc::write(const void *addr, size_t length)
{
    ssize_t rc = ::write(mFd, addr, length);
    if (rc == -1) {
        if (errno == EAGAIN)
            return 0;
        UnixError::throwMe();
    }
    return rc;
}


//
// Seeking
//
off_t FileDesc::seek(off_t position, int whence)
{
    off_t rc = ::lseek(mFd, position, whence);
    if (rc == -1)
        UnixError::throwMe();
    return rc;
}


//
// Mmap support
//
void *FileDesc::mmap(int prot, size_t length, int flags, off_t offset, void *addr)
{
    void *result = ::mmap(addr, length ? length : fileSize(), prot, flags, mFd, offset);
    if (result == MAP_FAILED)
        UnixError::throwMe();
    return result;
}


int FileDesc::fcntl(int cmd, int arg) const
{
    int rc = ::fcntl(mFd, cmd, arg);
    secdebug("unixio", "%d fcntl(%d,%d) = %d", mFd, cmd, arg, rc);
    if (rc == -1)
        UnixError::throwMe();
    return rc;
}

int FileDesc::fcntl(int cmd, void *arg) const
{
    // The BSD UNIX headers require an int argument to fcntl.
    // This will fail miserably if sizeof(void *) > sizeof(int). For such ports,
    // fix the problem here.
    assert(sizeof(void *) <= sizeof(int));
    return fcntl(cmd, reinterpret_cast<int>(arg));
}

int FileDesc::flags() const
{
    int flags = fcntl(F_GETFL);
    if (flags == -1)
        UnixError::throwMe();
    return flags;
}

void FileDesc::flags(int flags) const
{
    if (fcntl(F_SETFL, flags) == -1)
        UnixError::throwMe();
}

void FileDesc::setFlag(int flag, bool on) const
{
    if (flag) {		// if there's anything at all to do...
        int oldFlags = flags();
        flags(on ? (oldFlags | flag) : (oldFlags & ~flag));
    }
}

int FileDesc::ioctl(int cmd, void *arg) const
{
    int rc = ::ioctl(mFd, cmd, arg);
    if (rc == -1)
        UnixError::throwMe();
    return rc;
}


void FileDesc::fstat(UnixStat &st) const
{
    if (::fstat(mFd, &st))
        UnixError::throwMe();
}

size_t FileDesc::fileSize() const
{
    struct stat st;
    fstat(st);
    return st.st_size;
}


FILE *FileDesc::fdopen(const char *form)
{
    return ::fdopen(mFd, form);
}


//
// Signals and signal masks
//
SigSet sigMask(SigSet set, int how /* = SIG_SETMASK */)
{
	sigset_t old;
	checkError(::sigprocmask(how, &set.value(), &old));
	return old;
}


}	// end namespace IPPlusPlus
}	// end namespace Security
