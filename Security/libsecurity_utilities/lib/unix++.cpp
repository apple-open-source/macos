/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
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
// unix++ - C++ layer for basic UNIX facilities
//
#include "unix++.h"
#include <security_utilities/memutils.h>
#include <security_utilities/debugging.h>
#include <sys/xattr.h>
#include <cstdarg>


namespace Security {
namespace UnixPlusPlus {

using LowLevelMemoryUtilities::increment;


//
// Canonical open of a file descriptor. All other open operations channel through this.
// Note that we abuse the S_IFMT mode flags as operational options.
//
void FileDesc::open(const char *path, int flags, mode_t mode)
{
	if ((mFd = ::open(path, flags, mode & ~S_IFMT)) == -1) {
		if (errno == ENOENT && (mode & S_IFMT) == modeMissingOk) {
			return;
		} else {
			UnixError::throwMe();
        }
    }
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
// I/O with integral positioning.
// These don't affect file position and the atEnd() flag; and they
// don't make allowances for asynchronous I/O.
//
size_t FileDesc::read(void *addr, size_t length, size_t position)
{
	return checkError(::pread(mFd, addr, length, position));
}

size_t FileDesc::write(const void *addr, size_t length, size_t position)
{
	return checkError(::pwrite(mFd, addr, length, position));
}


//
// Waiting (repeating) I/O
//
size_t FileDesc::readAll(void *addr, size_t length)
{
	size_t total = 0;
	while (length > 0 && !atEnd()) {
		size_t size = read(addr, length);
		addr = increment(addr, size);
		length -= size;
		total += size;
	}
	return total;
}

size_t FileDesc::readAll(string &value)
{
	string s;
	while (!atEnd()) {
		char buffer[256];
		if (size_t size = read(buffer, sizeof(buffer))) {
			s += string(buffer, size);
			continue;
		}
	}
	swap(value, s);
	return value.length();
}


void FileDesc::writeAll(const void *addr, size_t length)
{
	while (length > 0) {
		size_t size = write(addr, length);
		addr = increment(addr, size);
		length -= size;
	}
}


//
// Seeking
//
#warning Cast to size_t may loose precision, only a problem for large files.

size_t FileDesc::seek(size_t position, int whence)
{
    return (size_t)checkError(::lseek(mFd, position, whence));
}

size_t FileDesc::position() const
{
	return (size_t)checkError(::lseek(mFd, 0, SEEK_CUR));
}


//
// Mmap support
//
void *FileDesc::mmap(int prot, size_t length, int flags, size_t offset, void *addr)
{
	if (!(flags & (MAP_PRIVATE | MAP_SHARED)))	// one is required
		flags |= MAP_PRIVATE;
    void *result = ::mmap(addr, length ? length : fileSize(), prot, flags, mFd, offset);
    if (result == MAP_FAILED)
        UnixError::throwMe();
    return result;
}


//
// Basic fcntl support
//
int FileDesc::fcntl(int cmd, void *arg) const
{
    int rc = ::fcntl(mFd, cmd, arg);
    secdebug("unixio", "%d fcntl(%d,%p) = %d", mFd, cmd, arg, rc);
	return checkError(rc);
}


//
// Nice fcntl forms
//
void FileDesc::setFlag(int flag, bool on) const
{
    if (flag) {		// if there's anything at all to do...
        int oldFlags = flags();
        flags(on ? (oldFlags | flag) : (oldFlags & ~flag));
    }
}


//
// Duplication operations
//
FileDesc FileDesc::dup() const
{
	return FileDesc(checkError(::dup(mFd)), atEnd());
}

FileDesc FileDesc::dup(int newFd) const
{
	return FileDesc(checkError(::dup2(mFd, newFd)), atEnd());
}


//
// Advisory locking, fcntl style
//
void FileDesc::lock(int type, const Pos &pos)
{
	LockArgs args(type, pos);
	IFDEBUG(args.debug(fd(), "lock"));
	checkError(fcntl(F_SETLKW, &args));
}

bool FileDesc::tryLock(int type, const Pos &pos)
{
	LockArgs args(type, pos);
	IFDEBUG(args.debug(fd(), "tryLock"));
	try {
		fcntl(F_SETLK, &args);
		return true;
	} catch (const UnixError &err) {
		if (err.error == EAGAIN)
			return false;
		else
			throw;
	}
}

#if !defined(NDEBUG)

void FileDesc::LockArgs::debug(int fd, const char *what)
{
	secdebug("fdlock", "%d %s %s:%ld(%ld)", fd, what,
		(l_whence == SEEK_SET) ? "ABS" : (l_whence == SEEK_CUR) ? "REL" : "END",
		long(l_start), long(l_len));
}

#endif //NDEBUG


//
// ioctl support
//
int FileDesc::ioctl(int cmd, void *arg) const
{
    int rc = ::ioctl(mFd, cmd, arg);
    if (rc == -1)
        UnixError::throwMe();
    return rc;
}


//
// Xattr support
//
void FileDesc::setAttr(const char *name, const void *value, size_t length,
	u_int32_t position /* = 0 */, int options /* = 0 */)
{
	checkError(::fsetxattr(mFd, name, value, length, position, options));
}

ssize_t FileDesc::getAttrLength(const char *name)
{
	ssize_t rc = ::fgetxattr(mFd, name, NULL, 0, 0, 0);
	if (rc == -1)
		switch (errno) {
		case ENOATTR:
			return -1;
		default:
			UnixError::throwMe();
		}
	return rc;
}

ssize_t FileDesc::getAttr(const char *name, void *value, size_t length,
	u_int32_t position /* = 0 */, int options /* = 0 */)
{
    ssize_t rc = ::fgetxattr(mFd, name, value, length, position, options);
	if (rc == -1)
		switch (errno) {
		case ENOATTR:
			return -1;
		default:
			UnixError::throwMe();
		}
	return rc;
}

void FileDesc::removeAttr(const char *name, int options /* = 0 */)
{
	if (::fremovexattr(mFd, name, options))
		switch (errno) {
		case ENOATTR:
			if (!(options & XATTR_REPLACE))	// somewhat mis-using an API flag here...
				return;		// attribute not found; we'll call that okay
			// fall through
		default:
			UnixError::throwMe();
		}
}

size_t FileDesc::listAttr(char *value, size_t length, int options /* = 0 */)
{
	return checkError(::flistxattr(mFd, value, length, options));
}


void FileDesc::setAttr(const std::string &name, const std::string &value, int options /* = 0 */)
{
	return setAttr(name, value.c_str(), value.size(), 0, options);
}

std::string FileDesc::getAttr(const std::string &name, int options /* = 0 */)
{
	char buffer[4096];	//@@@ auto-expand?
	ssize_t length = getAttr(name, buffer, sizeof(buffer), 0, options);
	if (length >= 0)
		return string(buffer, length);
	else
		return string();
}


//
// Stat support
//
void FileDesc::fstat(UnixStat &st) const
{
    if (::fstat(mFd, &st))
        UnixError::throwMe();
}

size_t FileDesc::fileSize() const
{
    struct stat st;
    fstat(st);
    return (size_t)st.st_size;
}

bool FileDesc::isA(int mode) const
{
	struct stat st;
	fstat(st);
	return (st.st_mode & S_IFMT) == mode;
}


void FileDesc::chown(uid_t uid)
{
	checkError(::fchown(mFd, uid, gid_t(-1)));
}

void FileDesc::chown(uid_t uid, gid_t gid)
{
	checkError(::fchown(mFd, uid, gid));
}

void FileDesc::chgrp(gid_t gid)
{
	checkError(::fchown(mFd, uid_t(-1), gid));
}

void FileDesc::chmod(mode_t mode)
{
	checkError(::fchmod(mFd, mode));
}

void FileDesc::chflags(u_int flags)
{
	checkError(::fchflags(mFd, flags));
}


FILE *FileDesc::fdopen(const char *form)
{
	//@@@ pick default value for 'form' based on chracteristics of mFd
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


//
// Make or use a directory, open-style.
//
// Flags are to be interpreted like open(2) flags; particularly
//	O_CREAT		make the directory if not present
//	O_EXCL		fail if the directory is present
// Other open(2) flags are currently ignored.
//
// Yes, it's a function.
//
void makedir(const char *path, int flags, mode_t mode)
{
	struct stat st;
	if (!stat(path, &st)) {
		if (flags & O_EXCL)
			UnixError::throwMe(EEXIST);
		if (!S_ISDIR(st.st_mode))
			UnixError::throwMe(ENOTDIR);
		secdebug("makedir", "%s exists", path);
		return;
	}

	// stat failed
	if (errno != ENOENT || !(flags & O_CREAT))
		UnixError::throwMe();
	
	// ENOENT and creation enabled
	if (::mkdir(path, mode)) {
		if (errno == EEXIST && !(flags & O_EXCL))
			return;		// fine (race condition, resolved)
		UnixError::throwMe();
	}
	secdebug("makedir", "%s created", path);
}


//
// Open, read/write, close a (small) file on disk
//
int ffprintf(const char *path, int flags, mode_t mode, const char *format, ...)
{
	FileDesc fd(path, flags, mode);
	FILE *f = fd.fdopen("w");
	va_list args;
	va_start(args, format);
	int rc = vfprintf(f, format, args);
	va_end(args);
	if (fclose(f))
		UnixError::throwMe();
	return rc;
}

int ffscanf(const char *path, const char *format, ...)
{
	if (FILE *f = fopen(path, "r")) {
		va_list args;
		va_start(args, format);
		int rc = vfscanf(f, format, args);
		va_end(args);
		if (!fclose(f))
			return rc;
	}
	UnixError::throwMe();
}


}	// end namespace IPPlusPlus
}	// end namespace Security
